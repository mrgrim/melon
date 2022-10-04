//
// Created by MrGrim on 10/4/2022.
//

#include <charconv>
#include <utility>
#include <cstring>
#include "primitive.h"
#include "snbt.h"

namespace melon::nbt
{
    tag_variant_t primitive::get_generic()
    {
        switch (type())
        {
            case tag_byte:
                return std::reference_wrapper<tag_access_t<tag_byte>>(value.tag_byte);
            case tag_short:
                return std::reference_wrapper<tag_access_t<tag_short>>(value.tag_short);
            case tag_int:
                return std::reference_wrapper<tag_access_t<tag_int>>(value.tag_int);
            case tag_long:
                return std::reference_wrapper<tag_access_t<tag_long>>(value.tag_long);
            case tag_float:
                return std::reference_wrapper<tag_access_t<tag_float>>(value.tag_float);
            case tag_double:
                return std::reference_wrapper<tag_access_t<tag_double>>(value.tag_double);
            case tag_string:
                return std::string_view(value.tag_string, size_v);
            case tag_byte_array:
                return std::span(value.tag_byte_array, size_v);
            case tag_int_array:
                return std::span(value.tag_int_array, size_v);
            case tag_long_array:
                return std::span(value.tag_long_array, size_v);
            default:
                std::unreachable();
        }
    }

    void primitive::to_snbt(std::string &out) const
    {
        // 3 = sign, decimal point, and nbt type indicator
        constexpr size_t buf_len = std::numeric_limits<double>::max_digits10 + std::numeric_limits<double>::max_exponent10 + 3;
        char             buf[buf_len];

        if (name != nullptr && !name->empty())
        {
            snbt::escape_string(*name, out, false);
            out.push_back(':');
        }

        if (tag_properties[type()].category == cat_primitive)
        {
            char                 suffix = 0;
            std::to_chars_result res; // NOLINT(cppcoreguidelines-pro-type-member-init)

            switch (type())
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
        else if (tag_properties[type()].category == cat_array)
        {
            auto print_array = [this, &buf, &out](auto array_ptr, char suffix = 0) {
                for (int32_t idx = 0; idx < size_v; idx++)
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

            switch (type())
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
        else if (tag_properties[type()].category == cat_string)
        {
            auto view = std::string_view(value.tag_string, size_v);
            snbt::escape_string(view, out);
        }
    }

    char *primitive::to_binary(char *itr) const
    {
        switch (tag_properties[type()].category)
        {
            case cat_primitive:
            {
                auto value_prep = util::cvt_endian<std::endian::little, std::endian::big>(value.generic);
                value_prep = util::pack_left<std::endian::little, std::endian::big>(value_prep, tag_properties[type()].size);

                std::memcpy(itr, static_cast<void *>(&value_prep), tag_properties[type()].size);
                itr += tag_properties[type()].size;

                return itr;
            }
            case cat_string:
            {
                auto len = util::cvt_endian<std::endian::little, std::endian::big>(static_cast<uint16_t>(size()));

                std::memcpy(itr, &len, sizeof(decltype(len)));
                itr += sizeof(decltype(len));

                std::memcpy(itr, value.tag_string, size());
                itr += size();

                return itr;
            }
            case cat_array:
            {
                auto count = util::cvt_endian<std::endian::little, std::endian::big>(static_cast<int32_t>(size()));

                std::memcpy(itr, &count, sizeof(decltype(count)));
                itr += sizeof(decltype(count));

                for (int index = 0; index < size(); index++)
                {
                    uint64_t value_prep;

                    std::memcpy(&value_prep, static_cast<char *>(value.generic_ptr) + (index * tag_properties[type()].size), sizeof(decltype(value_prep)));

                    value_prep = util::cvt_endian<std::endian::little, std::endian::big>(value_prep);
                    value_prep = util::pack_left<std::endian::little, std::endian::big>(value_prep, tag_properties[type()].size);

                    std::memcpy(itr, &value_prep, tag_properties[type()].size);
                    itr += tag_properties[type()].size;
                }

                return itr;
            }
            default:
                std::unreachable();
        }
    }
}