/* * * * * * * * * * * * * * libjwdpmi * * * * * * * * * * * * * */
/* Copyright (C) 2017 J.W. Jagersma, see COPYING.txt for details */
/* Copyright (C) 2016 J.W. Jagersma, see COPYING.txt for details */

#include <iostream>
#include <unordered_map>
#include <jw/io/detail/scancode.h>

//TODO: I don't really like all the dynamic allocation going on here.
namespace jw
{
    namespace io
    {
        namespace detail
        {
            key_state_pair scancode::decode()
            {
                key_state_pair k;
                k.second = key_state::down;

                bool ext0 = false;
                bool ext1 = false;

                for (auto c : sequence)
                {
                    if (c == 0xF0) { k.second = key_state::up; continue; }
                    switch (code_set)
                    {
                    case set1:
                    case set2:
                        if (c == 0xE0) { ext0 = true; continue; }
                        if (c == 0xE1) { ext1 = true; continue; }
                        if (ext0)
                        {
                            if (set2_extended0_to_key_table.count(c)) { k.first = set2_extended0_to_key_table[c]; continue; }
                            if (set2_extended0_to_set3_table.count(c)) { c = set2_extended0_to_set3_table[c]; }
                            else { k.first = 0xE000 + c; continue; }
                        }
                        else if (ext1)
                        {
                            if (c == 0x14) { k.first = key::pause; continue; }
                            else { k.first = 0xE100 + c; continue; }
                        }
                        else if (set2_to_set3_table.count(c)) { c = set2_to_set3_table[c]; }
                        [[fallthrough]];
                    case set3:
                        if (set3_to_key_table.count(c)) { k.first = set3_to_key_table[c]; }
                        else { k.first = 0x0100 + c; }
                    }
                }
                return k;
            }

            /*
            raw_scancode scancode::translate(raw_scancode c)
            {
                static const std::array<raw_scancode, 0x100> table = // from https://www.win.tue.nl/~aeb/linux/kbd/scancodes-10.html
                {
                0xFF, 0x43, 0x41, 0x3F, 0x3D, 0x3B, 0x3C, 0x58, 0x64, 0x44, 0x42, 0x40, 0x3E, 0x0F, 0x29, 0x59,
                0x65, 0x38, 0x2A, 0x70, 0x1D, 0x10, 0x02, 0x5A, 0x66, 0x71, 0x2C, 0x1F, 0x1E, 0x11, 0x03, 0x5B,
                0x67, 0x2E, 0x2D, 0x20, 0x12, 0x05, 0x04, 0x5C, 0x68, 0x39, 0x2F, 0x21, 0x14, 0x13, 0x06, 0x5D,
                0x69, 0x31, 0x30, 0x23, 0x22, 0x15, 0x07, 0x5E, 0x6A, 0x72, 0x32, 0x24, 0x16, 0x08, 0x09, 0x5F,
                0x6B, 0x33, 0x25, 0x17, 0x18, 0x0B, 0x0A, 0x60, 0x6C, 0x34, 0x35, 0x26, 0x27, 0x19, 0x0C, 0x61,
                0x6D, 0x73, 0x28, 0x74, 0x1A, 0x0D, 0x62, 0x6E, 0x3A, 0x36, 0x1C, 0x1B, 0x75, 0x2B, 0x63, 0x76,
                0x55, 0x56, 0x77, 0x78, 0x79, 0x7A, 0x0E, 0x7B, 0x7C, 0x4F, 0x7D, 0x4B, 0x47, 0x7E, 0x7F, 0x6F,
                0x52, 0x53, 0x50, 0x4C, 0x4D, 0x48, 0x01, 0x45, 0x57, 0x4E, 0x51, 0x4A, 0x37, 0x49, 0x46, 0x54,
                0x80, 0x81, 0x82, 0x41, 0x54, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
                0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
                0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
                0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
                0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
                0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
                0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
                0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
                };
                return table[c];
            }
            */

            const std::array<raw_scancode, 0x100> scancode::undo_translation_table
            {
                0xFF, 0x76, 0x16, 0x1E, 0x26, 0x25, 0x2E, 0x36, 0x3D, 0x3E, 0x46, 0x45, 0x4E, 0x55, 0x66, 0x0D,
                0x15, 0x1D, 0x24, 0x2D, 0x2C, 0x35, 0x3C, 0x43, 0x44, 0x4D, 0x54, 0x5B, 0x5A, 0x14, 0x1C, 0x1B,
                0x23, 0x2B, 0x34, 0x33, 0x3B, 0x42, 0x4B, 0x4C, 0x52, 0x0E, 0x12, 0x5D, 0x1A, 0x22, 0x21, 0x2A,
                0x32, 0x31, 0x3A, 0x41, 0x49, 0x4A, 0x59, 0x7C, 0x11, 0x29, 0x58, 0x05, 0x06, 0x04, 0x0C, 0x03,
                0x0B, 0x02, 0x83, 0x0A, 0x01, 0x09, 0x77, 0x7E, 0x6C, 0x75, 0x7D, 0x7B, 0x6B, 0x73, 0x74, 0x79,
                0x69, 0x72, 0x7A, 0x70, 0x71, 0x7F, 0x84, 0x60, 0x61, 0x78, 0x07, 0x0F, 0x17, 0x1F, 0x27, 0x2F,
                0x37, 0x3F, 0x47, 0x4F, 0x56, 0x5E, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40, 0x48, 0x50,
                0x57, 0x6F, 0x13, 0x19, 0x39, 0x51, 0x53, 0x5C, 0x5F, 0x62, 0x63, 0x64, 0x65, 0x67, 0x68, 0x6A,
                0x6D, 0x6E, 0x80, 0x81, 0x82, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
                0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
                0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
                0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
                0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
                0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
                0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
                0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0x00
            };

            std::unordered_map<raw_scancode, raw_scancode> scancode::set2_to_set3_table
            {
                { 0x01, 0x47 }, { 0x03, 0x27 }, { 0x04, 0x17 }, { 0x05,0x07 },
                { 0x06, 0x0F }, { 0x07, 0x5E }, { 0x09, 0x4F }, { 0x0A,0x3F },
                { 0x0B, 0x2F }, { 0x0C, 0x1F }, { 0x11, 0x19 }, { 0x14,0x11 },
                { 0x58, 0x14 }, { 0x5D, 0x5C }, { 0x76, 0x08 }, { 0x77,0x76 },
                { 0x78, 0x56 }, { 0x79, 0x7C }, { 0x7B, 0x84 }, { 0x7C,0x7E },
                { 0x7E, 0x5F }, { 0x83, 0x37 }, { 0x84, 0x57 }
            };

            std::unordered_map<raw_scancode, const raw_scancode> scancode::set2_extended0_to_set3_table
            {
                { 0x11, 0x39 }, { 0x14, 0x58 }, { 0x1F, 0x8B }, { 0x27, 0x8C },
                { 0x2F, 0x8D }, { 0x3F, 0x7F }, { 0x4A, 0x77 }, { 0x5A, 0x79 },
                { 0x69, 0x65 }, { 0x6B, 0x61 }, { 0x6C, 0x6E }, { 0x70, 0x67 },
                { 0x71, 0x64 }, { 0x72, 0x60 }, { 0x74, 0x6A }, { 0x75, 0x63 },
                { 0x7A, 0x6D }, { 0x7C, 0x57 }, { 0x7D, 0x6F }, { 0x7E, 0x62 }
            };

            std::unordered_map<raw_scancode, const raw_scancode> scancode::set2_extended0_to_key_table
            {
                { 0x37, key::pwr_on },
                { 0x5E, key::pwr_wake }
            };

            std::unordered_map<raw_scancode, key> scancode::set3_to_key_table
            {
                { 0x00, key::bad_key },
                //{ 0x01, key:: },
                //{ 0x02, key:: },
                //{ 0x03, key:: },
                //{ 0x04, key:: },
                //{ 0x05, key:: },
                //{ 0x06, key:: },
                { 0x07, key::f1 },
                { 0x08, key::esc },
                //{ 0x09, key:: },
                //{ 0x0A, key:: },
                //{ 0x0B, key:: },
                //{ 0x0C, key:: },
                { 0x0D, key::tab },
                { 0x0E, key::backtick },
                { 0x0F, key::f2 },
                //{ 0x10, key:: },
                { 0x11, key::ctrl_left },
                { 0x12, key::shift_left },
                //{ 0x13, key:: },
                { 0x14, key::caps_lock },
                { 0x15, key::q },
                { 0x16, key::n1 },
                { 0x17, key::f3 },
                //{ 0x18, key:: },
                { 0x19, key::alt_left },
                { 0x1A, key::z },
                { 0x1B, key::s },
                { 0x1C, key::a },
                { 0x1D, key::w },
                { 0x1E, key::n2 },
                { 0x1F, key::f4 },
                //{ 0x20, key:: },
                { 0x21, key::c },
                { 0x22, key::x },
                { 0x23, key::d },
                { 0x24, key::e },
                { 0x25, key::n4 },
                { 0x26, key::n3 },
                { 0x27, key::f5 },
                //{ 0x28, key:: },
                { 0x29, key::space },
                { 0x2A, key::v },
                { 0x2B, key::f },
                { 0x2C, key::t },
                { 0x2D, key::r },
                { 0x2E, key::n5 },
                { 0x2F, key::f6 },
                //{ 0x30, key:: },
                { 0x31, key::n },
                { 0x32, key::b },
                { 0x33, key::h },
                { 0x34, key::g },
                { 0x35, key::y },
                { 0x36, key::n6 },
                { 0x37, key::f7 },
                //{ 0x38, key:: },
                { 0x39, key::alt_right },
                { 0x3A, key::m },
                { 0x3B, key::j },
                { 0x3C, key::u },
                { 0x3D, key::n7 },
                { 0x3E, key::n8 },
                { 0x3F, key::f8 },
                //{ 0x40, key:: },
                { 0x41, key::comma },
                { 0x42, key::k },
                { 0x43, key::i },
                { 0x44, key::o },
                { 0x45, key::n0 },
                { 0x46, key::n9 },
                { 0x47, key::f9 },
                //{ 0x48, key:: },
                { 0x49, key::dot },
                { 0x4A, key::slash },
                { 0x4B, key::l },
                { 0x4C, key::semicolon },
                { 0x4D, key::p },
                { 0x4E, key::minus },
                { 0x4F, key::f10 },
                //{ 0x50, key:: },
                //{ 0x51, key:: },
                { 0x52, key::quote },
                //{ 0x53, key:: },
                { 0x54, key::brace_left },
                { 0x55, key::equals },
                { 0x56, key::f11 },
                { 0x57, key::print_screen },
                { 0x58, key::ctrl_right },
                { 0x59, key::shift_right },
                { 0x5A, key::enter },
                { 0x5B, key::brace_right },
                { 0x5C, key::backslash },
                //{ 0x5D, key:: },
                { 0x5E, key::f12 },
                { 0x5F, key::scroll_lock },
                { 0x60, key::down },
                { 0x61, key::left },
                { 0x62, key::pause },
                { 0x63, key::up },
                { 0x64, key::del },
                { 0x65, key::end },
                { 0x66, key::backspace },
                { 0x67, key::insert },
                //{ 0x68, key:: },
                { 0x69, key::num_1 },
                { 0x6A, key::right },
                { 0x6B, key::num_4 },
                { 0x6C, key::num_7 },
                { 0x6D, key::page_down },
                { 0x6E, key::home },
                { 0x6F, key::page_up },
                { 0x70, key::num_0 },
                { 0x71, key::num_dot },
                { 0x72, key::num_2 },
                { 0x73, key::num_5 },
                { 0x74, key::num_6 },
                { 0x75, key::num_8 },
                { 0x76, key::num_lock },
                { 0x77, key::num_div },
                //{ 0x78, key:: },
                { 0x79, key::num_enter },
                { 0x7A, key::num_3 },
                //{ 0x7B, key:: },
                { 0x7C, key::num_add },
                { 0x7D, key::num_9 },
                { 0x7E, key::num_mul },
                { 0x7F, key::pwr_sleep },
                //{ 0x80, key:: },
                //{ 0x81, key:: },
                //{ 0x82, key:: },
                //{ 0x83, key:: },
                { 0x84, key::num_sub },
                //{ 0x85, key:: },
                //{ 0x86, key:: },
                //{ 0x87, key:: },
                //{ 0x88, key:: },
                //{ 0x89, key:: },
                //{ 0x8A, key:: },
                { 0x8B, key::win_left },
                { 0x8C, key::win_right },
                { 0x8D, key::win_menu },
                //{ 0x8E, key:: },
                //{ 0x8F, key:: },
            };
        }
    }
}
