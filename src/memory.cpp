/* * * * * * * * * * * * * * libjwdpmi * * * * * * * * * * * * * */
/* Copyright (C) 2017 J.W. Jagersma, see COPYING.txt for details */
/* Copyright (C) 2016 J.W. Jagersma, see COPYING.txt for details */

#include <jw/dpmi/memory.h>

namespace jw
{
    namespace dpmi
    {
        bool memory_base::new_alloc_supported { true };
        bool device_memory_base::device_map_supported { true };
        bool mapped_dos_memory_base::dos_map_supported { true };

        ldt_access_rights ldt_entry::get_access_rights() { return ldt_access_rights { sel }; }

        void memory_base::old_alloc()
        {
            throw_if_irq();
            if (handle != null_handle) deallocate();
            split_uint32_t new_size { size };
            split_uint32_t new_addr, new_handle;
            dpmi_error_code error;
            bool c;
            do
            {
                asm volatile(
                    "int 0x31;"
                    : "=@ccc" (c)
                    , "=a" (error)
                    , "=b" (new_addr.hi)
                    , "=c" (new_addr.lo)
                    , "=S" (new_handle.hi)
                    , "=D" (new_handle.lo)
                    : "a" (0x0501)
                    , "b" (new_size.hi)
                    , "c" (new_size.lo)
                    : "memory");
                if (c) throw dpmi_error(error, __PRETTY_FUNCTION__);
            } while (!is_valid_address(new_addr));
            handle = new_handle;
            addr = new_addr;
        }

        void memory_base::new_alloc(bool committed, std::uintptr_t desired_address)
        {
            if (committed) throw_if_irq();
            if (handle != null_handle) deallocate();
            std::uint32_t new_handle;
            std::uintptr_t new_addr;
            dpmi_error_code error;
            bool c;
            do
            {
                asm volatile(
                    "int 0x31;"
                    : "=@ccc" (c)
                    , "=a" (error)
                    , "=b" (new_addr)
                    , "=S" (new_handle)
                    : "a" (0x0504)
                    , "b" (desired_address)
                    , "c" (size)
                    , "d" (static_cast<std::uint32_t>(committed))
                    : "memory");
                if (c) throw dpmi_error(error, __PRETTY_FUNCTION__);
            } while (!is_valid_address(new_addr));
            handle = new_handle;
            addr = new_addr;
        }

        void memory_base::old_resize(std::size_t num_bytes)
        {
            throw_if_irq();
            split_uint32_t new_size { num_bytes };
            split_uint32_t new_handle { handle };
            split_uint32_t new_addr;
            dpmi_error_code error;
            bool c;
            do
            {
                asm volatile(
                    "int 0x31;"
                    : "=@ccc" (c)
                    , "=a" (error)
                    , "=b" (new_addr.hi)
                    , "=c" (new_addr.lo)
                    , "+S" (new_handle.hi)
                    , "+D" (new_handle.lo)
                    : "a" (0x0503)
                    , "b" (new_size.hi)
                    , "c" (new_size.lo)
                    : "memory");
                if (c) throw dpmi_error(error, __PRETTY_FUNCTION__);
            } while (!is_valid_address(new_addr));
            handle = new_handle;
            addr = new_addr;
            size = new_size;
        }

        void memory_base::new_resize(std::size_t num_bytes, bool committed)
        {
            if (committed) throw_if_irq();
            std::uint32_t new_handle { handle };
            std::uintptr_t new_addr;
            dpmi_error_code error;
            bool c;
            do
            {
                asm volatile(
                    "int 0x31;"
                    : "=@ccc" (c)
                    , "=a" (error)
                    , "=b" (new_addr)
                    , "+S" (new_handle)
                    : "a" (0x0505)
                    , "c" (num_bytes)
                    , "d" (static_cast<std::uint32_t>(committed))
                    : "memory");
                if (c) throw dpmi_error(error, __PRETTY_FUNCTION__);
            } while (!is_valid_address(new_addr));
            handle = new_handle;
            addr = new_addr;
        }

        void device_memory_base::old_alloc(std::uintptr_t physical_address)
        {
            throw_if_irq();
            split_uint32_t new_addr;
            split_uint32_t new_size { size };
            split_uint32_t phys { physical_address };
            dpmi_error_code error;
            bool c;
            asm volatile(
                "int 0x31;"
                : "=@ccc" (c)
                , "=a" (error)
                , "=b" (new_addr.hi)
                , "=c" (new_addr.lo)
                : "a" (0x0800)
                , "b" (phys.hi)
                , "c" (phys.lo)
                , "S" (new_size.hi)
                , "D" (new_size.lo)
                : "memory");
            if (c) throw dpmi_error(error, __PRETTY_FUNCTION__);
            addr = new_addr;
        }

        void device_memory_base::new_alloc(std::uintptr_t physical_address)
        {
            auto addr_start = round_down_to_page_size(physical_address);
            auto offset = physical_address - addr_start;
            auto pages = round_up_to_page_size(size) / get_page_size();
            auto offset_in_block = round_up_to_page_size(addr) - addr;
            offset += offset_in_block;
            addr += offset;
            size -= offset;
            dpmi_error_code error;
            bool c;
            asm volatile(
                "int 0x31;"
                : "=@ccc" (c)
                , "=a" (error)
                : "a" (0x0508)
                , "b" (offset_in_block)
                , "c" (pages)
                , "d" (addr_start)
                , "S" (handle)
                : "memory");
            if (c) throw dpmi_error(error, __PRETTY_FUNCTION__);
        }

        void mapped_dos_memory_base::new_alloc(std::uintptr_t dos_linear_address)
        {
            auto addr_start = round_down_to_page_size(dos_linear_address);
            offset = dos_linear_address - addr_start;
            auto pages = round_up_to_page_size(size) / get_page_size();
            auto offset_in_block = round_up_to_page_size(addr) - addr;
            addr += offset + offset_in_block;
            size -= offset + offset_in_block;
            dpmi_error_code error;
            bool c;
            asm volatile(
                "int 0x31;"
                : "=@ccc" (c)
                , "=a" (error)
                : "a" (0x0509)
                , "b" (offset_in_block)
                , "c" (pages)
                , "d" (addr_start)
                , "S" (handle)
                : "memory");
            if (c) throw dpmi_error(error, __PRETTY_FUNCTION__);
        }

        void mapped_dos_memory_base::alloc_selector()
        {
            selector sel;
            bool c;
            asm volatile(
                "int 0x31;"
                : "=@ccc" (c)
                , "=a" (sel)
                : "a" (0x0002)
                , "b" (dos_addr.segment)
                : "memory");
            if (c) throw dpmi_error(sel, __PRETTY_FUNCTION__);
            dos_handle = sel;
        }

        void dos_memory_base::dos_alloc(std::size_t num_bytes)
        {
            throw_if_irq();
            std::uint16_t new_handle;
            far_ptr16 new_addr { };
            bool c;
            asm volatile(
                "int 0x31;"
                : "=@ccc" (c)
                , "=a" (new_addr.segment)
                , "=d" (new_handle)
                : "a" (0x0100)
                , "b" (bytes_to_paragraphs(num_bytes))
                : "memory");
            if (c) throw dpmi_error(new_addr.segment, __PRETTY_FUNCTION__);
            dos_handle = new_handle;
            dos_addr = new_addr;
        }

        void dos_memory_base::dos_dealloc()
        {
            throw_if_irq();
            dpmi_error_code error;
            bool c;
            asm volatile(
                "int 0x31;"
                : "=@ccc" (c)
                , "=a" (error)
                : "a" (0x0101)
                , "d" (dos_handle)
                : "memory");
            if (c) throw dpmi_error(error, __PRETTY_FUNCTION__);
            dos_handle = null_dos_handle;
        }

        void dos_memory_base::dos_resize(std::size_t num_bytes)
        {
            throw_if_irq();
            dpmi_error_code error;
            bool c;
            asm volatile(
                "int 0x31;"
                : "=@ccc" (c)
                , "=a" (error)
                : "a" (0x0102)
                , "b" (bytes_to_paragraphs(num_bytes))
                , "d" (dos_handle)
                : "memory");
            if (c) throw dpmi_error(error, __PRETTY_FUNCTION__);
            size = num_bytes;
        }
    }
}
