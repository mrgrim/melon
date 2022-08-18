//
// Created by MrGrim on 8/14/2022.
//

#include <string>
#include <fstream>
#include <vector>
#include <iostream>
#include "file.h"

namespace melon::util {
    int file_to_vec(const std::string& path, std::vector<char>& out) {
        std::basic_ifstream<char> file_stream;

        file_stream.open(path, std::ios::binary | std::ios::ate);

        if (!file_stream)
            return -1;

        int64_t size = file_stream.tellg();
        file_stream.seekg(0, std::ios::beg);
        out.resize(size);

        if (!file_stream.read(out.data(), size))
            return -1;

        return 0;
    }
}