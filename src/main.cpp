#include <iostream>
#include <vector>
#include <cstring>
#include <memory>
#include <chrono>

#include "util/deflate.h"
#include "util/file.h"
#include "nbt/nbt.h"

int main() {
    std::vector<char> gz_buffer;

    auto start = std::chrono::high_resolution_clock::now();

    if (melon::util::file_to_vec(R"(E:\Games\Minecraft\Servers\Fabric\World\level.dat)", gz_buffer))
    {
        std::cerr << "Unable to load level.dat: " << strerror(errno) << std::endl;
        return 1;
    }

    std::cout << "Successfully read file off disk." << std::endl;
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start) << std::endl;

    std::vector<uint8_t> nbt_data;
    if (!melon::util::gunzip(gz_buffer, &nbt_data))
    {
        std::cout << "Successfully decompressed NBT data (" << nbt_data.size() << " bytes)." << std::endl;
        end = std::chrono::high_resolution_clock::now();
        std::cout << "Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start) << std::endl;
    }
    else
    {
        return 1;
    }

    melon::nbt::compound parsed_nbt;
    parsed_nbt.read(&nbt_data);
    std::cout << "Successfully parsed NBT data." << std::endl;

    end = std::chrono::high_resolution_clock::now();
    std::cout << "Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start) << std::endl;

    return 0;
}
