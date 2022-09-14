//
// Created by MrGrim on 8/16/2022.
//

#include <charconv>
#include "nbt/nbt.h"

namespace melon::nbt
{

    void primitive_tag::to_snbt(std::string &out)
    {
        // 3 = sign, decimal point, and nbt type indicator
        constexpr size_t buf_len = std::numeric_limits<double>::max_digits10 + std::numeric_limits<double>::max_exponent10 + 3;
        char             buf[buf_len];

        if (name != nullptr && !name->empty())
        {
            snbt::escape_string(*name, out, false);
            out.push_back(':');
        }

        if (tag_properties[tag_type].category == cat_primitive)
        {
            char                 suffix = 0;
            std::to_chars_result res{};

            switch (tag_type)
            {
                // @formatter:off
                case tag_byte: res = std::to_chars(buf, buf + buf_len, value.tag_byte); suffix = 'b'; break;
                case tag_short: res = std::to_chars(buf, buf + buf_len, value.tag_short); suffix = 's'; break;
                case tag_int: res = std::to_chars(buf, buf + buf_len, value.tag_int); break;
                case tag_long: res = std::to_chars(buf, buf + buf_len, value.tag_long); suffix = 'L'; break;
                case tag_float: res = std::to_chars(buf, buf + buf_len, value.tag_float, std::chars_format::fixed); suffix = 'f'; break;
                case tag_double: res = std::to_chars(buf, buf + buf_len, value.tag_double, std::chars_format::fixed); suffix = 'd'; break;
                default: std::unreachable();
                // @formatter:on
            }

            if (res.ec == std::errc())
            {
                out.append(buf, res.ptr);
                if (suffix) out.push_back(suffix);
            }
            else
                [[unlikely]] throw std::runtime_error("Error converting NBT primitive to string: " + std::make_error_code(res.ec).message());
        }
        else if (tag_properties[tag_type].category == cat_array)
        {
            auto print_array = [this, &buf, &out](auto array_ptr, char suffix = 0) {
                for (uint32_t idx = 0; idx < size_v; idx++)
                {
                    if (auto [ptr, ec] = std::to_chars(buf, buf + buf_len, array_ptr[idx]); ec == std::errc())
                    {
                        out.append(buf, ptr);
                        if (suffix) out.push_back(suffix);

                        if (idx != (size_v - 1)) out.push_back(',');
                    }
                    else
                        [[unlikely]] throw std::runtime_error("Error converting NBT primitive to string: " + std::make_error_code(ec).message());
                }
            };

            switch (tag_type)
            {
                // @formatter:off
                case tag_byte_array: out.append("[B;"); print_array(value.tag_byte_array, 'B'); break;
                case tag_int_array: out.append("[I;"); print_array(value.tag_int_array); break;
                case tag_long_array: out.append("[L;"); print_array(value.tag_long_array, 'L'); break;
                default: std::unreachable();
                // @formatter:on
            }

            out.push_back(']');
        }
        else if (tag_properties[tag_type].category == cat_string)
        {
            auto view = std::string_view(value.tag_string, size_v);
            snbt::escape_string(view, out);
        }
    }

    namespace snbt
    {
        void escape_string(const std::string_view &in_str, std::string &out_str, bool always_quote)
        {
            if (!always_quote && !in_str.empty() && in_str.find_first_not_of(snbt::syntax::string_unquoted_chars) == std::string_view::npos)
            {
                // No quoting or escaping required
                out_str.append(in_str);
            }
            else
            {
                char using_quote = 0;

                auto start = out_str.size();
                out_str.push_back(' ');

                size_t last_pos = 0;
                auto   esc_pos  = in_str.find_first_of(syntax::string_chars_to_escape);

                while (esc_pos != std::string_view::npos)
                {
                    auto c = in_str[esc_pos];
                    if (!using_quote && (c == syntax::string_std_quote || c == syntax::string_alt_quote))
                        using_quote = (c == syntax::string_std_quote) ? syntax::string_alt_quote : syntax::string_std_quote;

                    out_str.append(in_str.substr(last_pos, esc_pos - last_pos));
                    if (c == using_quote || c == syntax::string_escape_char) out_str.push_back(syntax::string_escape_char);

                    last_pos = esc_pos;
                    esc_pos  = in_str.find_first_of(syntax::string_chars_to_escape, esc_pos + 1);
                }

                if (!using_quote) using_quote = syntax::string_std_quote;

                out_str.append(in_str.substr(last_pos, in_str.size() - last_pos));
                out_str.push_back(using_quote);
                out_str[start] = using_quote;
            }
        }
    }
}
