//
// Created by MrGrim on 10/4/2022.
//

#include <charconv>
#include <string>
#include "snbt.h"

namespace melon::nbt::snbt
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
