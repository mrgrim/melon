//
// Created by MrGrim on 8/14/2022.
//

#ifndef LODE_UTIL_FILE_H
#define LODE_UTIL_FILE_H

#include <optional>

namespace melon::util
{
    std::optional<std::pair<std::unique_ptr<char[]>, size_t>> file_to_buf(std::string_view path);
    bool buf_to_file(std::string_view path, std::unique_ptr<char[]> &&buf, size_t size, std::ios_base::openmode extra_flags);
}

#endif //LODE_UTIL_FILE_H
