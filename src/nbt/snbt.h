//
// Created by MrGrim on 10/4/2022.
//

#ifndef MELON_NBT_SNBT_H
#define MELON_NBT_SNBT_H

#include <string_view>

namespace melon::nbt::snbt
{
    void escape_string(const std::string_view &in_str, std::string &out_str, bool always_quote = true);

    namespace syntax
    {
        constexpr std::string_view string_unquoted_chars  = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxzy+-_.";
        constexpr std::string_view string_chars_to_escape = R"("'\)";
        constexpr char             string_escape_char     = '\\';
        constexpr char             string_std_quote       = '"';
        constexpr char             string_alt_quote       = '\'';
    }
}

#endif //MELON_NBT_SNBT_H
