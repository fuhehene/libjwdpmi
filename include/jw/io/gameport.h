/* * * * * * * * * * * * * * libjwdpmi * * * * * * * * * * * * * */
/* Copyright (C) 2018 J.W. Jagersma, see COPYING.txt for details */

#pragma once
#include <jw/io/ioport.h>
#include <jw/chrono/chrono.h>
#include <jw/dpmi/irq.h>
#include <jw/thread/task.h>
#include <jw/dpmi/lock.h>
#include <jw/dpmi/alloc.h>
#include <jw/event.h>
#include <limits>
#include <optional>
#include <experimental/deque>

// TODO: centering
// TODO: auto-calibrate?

namespace jw::io
{
    struct gameport
    {
        enum class poll_strategy
        {
            busy_loop,
            pit_irq,
            rtc_irq,
            thread
        };

        struct config
        {
            port_num port { 0x201 };
            poll_strategy strategy { poll_strategy::busy_loop };
            chrono::tsc::duration smoothing_window { std::chrono::milliseconds { 50 } };

            struct
            {
                bool x0 { true }, y0 { true };
                bool x1 { true }, y1 { true };
            } enable;

            struct
            {
                chrono::tsc_count x0_min { 0 };
                chrono::tsc_count y0_min { 0 };
                chrono::tsc_count x1_min { 0 };
                chrono::tsc_count y1_min { 0 };
                chrono::tsc_count x0_max { std::numeric_limits<chrono::tsc_count>::max() };
                chrono::tsc_count y0_max { std::numeric_limits<chrono::tsc_count>::max() };
                chrono::tsc_count x1_max { std::numeric_limits<chrono::tsc_count>::max() };
                chrono::tsc_count y1_max { std::numeric_limits<chrono::tsc_count>::max() };
            } calibration;

            struct
            {
                float x0_min { -1 };
                float x0_max { +1 };
                float y0_min { -1 };
                float y0_max { +1 };
                float x1_min { -1 };
                float x1_max { +1 };
                float y1_min { -1 };
                float y1_max { +1 };
            } output_range;
        };

        template<typename T>
        struct value_t
        {
            T x0, y0, x1, y1;

            constexpr value_t(T vx0, T vy0, T vx1, T vy1) noexcept : x0(vx0), y0(vy0), x1(vx1), y1(vy1) { }
            constexpr value_t(T v) noexcept : value_t(v, v, v, v) { }
            constexpr value_t() noexcept = default;
        };

        template<typename T>
        struct vector_t
        {
            using V[[gnu::vector_size(2 * sizeof(T))]] = T;
            union
            {
                V v;
                struct { T x0, y0, x1, y1; };
            };

            constexpr vector_t(T vx0, T vy0, T vx1, T vy1) noexcept : x0(vx0), y0(vy0), x1(vx1), y1(vy1) { }
            constexpr vector_t(T v) noexcept : vector_t(v, v, v, v) { }
            constexpr vector_t() noexcept = default;
        };

        using raw_t = value_t<chrono::tsc_count>;
        using normalized_t = vector_t<float>;

        struct button_t
        {
            bool a0, b0, a1, b1;
            constexpr bool operator==(const button_t& o) const noexcept { return a0 == o.a0 and b0 == o.b0 and a1 == o.a1 and b1 == o.b1; }
            constexpr bool operator!=(const button_t& o) const noexcept { return not (o == *this); }
        };

        gameport(config c, std::size_t alloc_size = 1_KB) : cfg(c), port(c.port),
            memory_resource(using_irq()? std::make_unique<dpmi::locked_pool_memory_resource>(alloc_size) : nullptr)
        {
            switch (cfg.strategy)
            {
            case poll_strategy::pit_irq:
                poll_irq.set_irq(0);
                break;
            case poll_strategy::rtc_irq:
                poll_irq.set_irq(8);
                break;
            default:
                break;
            }
            if (using_irq())
            {
                lock = std::make_optional<dpmi::data_lock>(this);
                poll_irq.enable();
            }
            poll_task->start();
        }

        ~gameport()
        {
            poll_irq.disable();
            if (poll_task->is_running()) poll_task->abort();
        }

        auto get_raw()
        {
            auto now = chrono::tsc::now();
            for (auto i = samples.begin(); i != samples.end();)
            {
                if (samples.size() > 1 and chrono::tsc::to_time_point(i->second) < now - cfg.smoothing_window)
                    i = samples.erase(i);
                else ++i;
            }
            poll();
            return samples.back().first;
        }

        auto get()
        {
            get_raw();
            normalized_t value { 0 };

            auto& c = cfg.calibration;
            auto& o = cfg.output_range;

            for (auto&& s : samples)
            {
                value.x0 += s.first.x0;     // TODO: generic vector4 class
                value.y0 += s.first.y0;
                value.x1 += s.first.x1;
                value.y1 += s.first.y1;
            }
            value.x0 /= samples.size();
            value.y0 /= samples.size();
            value.x1 /= samples.size();
            value.y1 /= samples.size();

            value.x0 = (value.x0 - c.x0_min) / (c.x0_max - c.x0_min);
            value.y0 = (value.y0 - c.y0_min) / (c.y0_max - c.y0_min);
            value.x1 = (value.x1 - c.x1_min) / (c.x1_max - c.x1_min);
            value.y1 = (value.y1 - c.y1_min) / (c.y1_max - c.y1_min);

            value.x0 = o.x0_min + value.x0 * (o.x0_max - o.x0_min);
            value.y0 = o.y0_min + value.y0 * (o.y0_max - o.y0_min);
            value.x1 = o.x1_min + value.x1 * (o.x1_max - o.x1_min);
            value.y1 = o.y1_min + value.y1 * (o.y1_max - o.y1_min);

            return value;
        }

        auto buttons()
        {
            if (cfg.strategy != poll_strategy::busy_loop) poll();
            return button_state;
        }

        event<void(button_t, chrono::tsc::time_point)> button_changed;

    private:
        struct [[gnu::packed]] raw_gameport
        {
            bool x0 : 1;
            bool y0 : 1;
            bool x1 : 1;
            bool y1 : 1;
            bool a0 : 1;
            bool b0 : 1;
            bool a1 : 1;
            bool b1 : 1;
        };
        static_assert(sizeof(raw_gameport) == 1);

        const config cfg;
        io_port<raw_gameport> port;
        raw_t last;
        value_t<bool> timing { false };
        chrono::tsc_count timing_start;
        button_t button_state;
        std::optional<dpmi::data_lock> lock;
        std::unique_ptr<std::experimental::pmr::memory_resource> memory_resource;
        std::experimental::pmr::deque<std::pair<button_t, chrono::tsc_count>> button_events { get_memory_resource() };
        std::experimental::pmr::deque<std::pair<raw_t, chrono::tsc_count>> samples { get_memory_resource() };

        bool using_irq() const { return cfg.strategy == poll_strategy::pit_irq or cfg.strategy == poll_strategy::rtc_irq; }
        std::experimental::pmr::memory_resource* get_memory_resource() const noexcept { if (using_irq()) return memory_resource.get(); else return std::experimental::pmr::get_default_resource(); }

        void update_buttons(raw_gameport p, chrono::tsc_count now)
        {
            button_t x;
            x.a0 = not p.a0;
            x.b0 = not p.b0;
            x.a1 = not p.a1;
            x.b1 = not p.b1;
            if (x != button_state) button_events.emplace_back(x, now);
            button_state = x;
        }

        void poll()
        {
            if (not timing.x0 and not timing.y0 and not timing.x1 and not timing.y1)
            {
                timing.x0 = cfg.enable.x0;
                timing.y0 = cfg.enable.y0;
                timing.x1 = cfg.enable.x1;
                timing.y1 = cfg.enable.y1;
                port.write({ });
                timing_start = chrono::rdtsc();
            }
            do
            {
                auto p = port.read();
                auto now = chrono::rdtsc();
                auto& c = cfg.calibration;
                auto i = now - timing_start;
                if (timing.x0 and (not p.x0 or i > c.x0_max)) { timing.x0 = false; last.x0 = std::clamp(i, c.x0_min, c.x0_max); }
                if (timing.y0 and (not p.y0 or i > c.y0_max)) { timing.y0 = false; last.y0 = std::clamp(i, c.y0_min, c.y0_max); }
                if (timing.x1 and (not p.x1 or i > c.x1_max)) { timing.x1 = false; last.x1 = std::clamp(i, c.x1_min, c.x1_max); }
                if (timing.y1 and (not p.y1 or i > c.y1_max)) { timing.y1 = false; last.y1 = std::clamp(i, c.y1_min, c.y1_max); }
                update_buttons(p, now);
            } while (cfg.strategy == poll_strategy::busy_loop and (timing.x0 or timing.y0 or timing.x1 or timing.y1));
            if (not (timing.x0 or timing.y0 or timing.x1 or timing.y1)) samples.emplace_back(last, chrono::rdtsc());
        }

        thread::task<void()> poll_task { [this]
        {
            while (true)
            {
                if (cfg.strategy != poll_strategy::busy_loop) poll();
                for (auto i = button_events.begin(); i != button_events.end(); i = button_events.erase(i))
                    button_changed(i->first, chrono::tsc::to_time_point(i->second));
                thread::yield();
            }
        } };

        dpmi::irq_handler poll_irq { [this]
        {
            poll();
        }, dpmi::always_call };
    };
}
