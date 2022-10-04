//
// Created by MrGrim on 8/14/2022.
//

#include <string_view>
#include <filesystem>
#include <fstream>
#include <optional>
#include <memory>
#include "file.h"

namespace melon::util {
    std::optional<std::pair<std::unique_ptr<char[]>, size_t>> file_to_buf(const std::string_view path) {
        std::basic_ifstream<char> file_stream;

        file_stream.open(std::filesystem::path(path), std::ios::binary | std::ios::ate);

        if (!file_stream)
            return std::nullopt;

        int64_t size = file_stream.tellg();
        file_stream.seekg(0, std::ios::beg);

        auto ptr = std::make_unique<char[]>(size);

        if (!file_stream.read(ptr.get(), size))
            return std::nullopt;

        return std::pair { std::move(ptr), size };
    }

    bool buf_to_file(const std::string_view path, std::unique_ptr<char[]> &&buf, size_t size, std::ios_base::openmode extra_flags)
    {
        std::basic_ofstream<char> file_stream;

        file_stream.open(std::filesystem::path(path), std::ios::binary | extra_flags);

        if (!file_stream)
            return false;

        if (!file_stream.write(buf.get(), size))
            return false;

        return true;
    }
}