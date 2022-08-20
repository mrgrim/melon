#include <iostream>
#include <vector>
#include <cstring>
#include <memory>
#include <chrono>

#include "util/deflate.h"
#include "util/file.h"
#include "nbt/compound.h"

int main() {
    std::vector<char> gz_buffer;

    auto start = std::chrono::high_resolution_clock::now();

    if (melon::util::file_to_vec(R"(E:\Games\Minecraft\Servers\Fabric\World\level.dat)", gz_buffer))
    {
        std::cerr << "Unable to load level.dat: " << strerror(errno) << std::endl;
        return 1;
    }

    auto end = std::chrono::high_resolution_clock::now();

    std::cout << "Successfully read file off disk." << std::endl;
    std::cout << "Elapsed time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start) << std::endl;

    start = std::chrono::high_resolution_clock::now();

    std::vector<uint8_t> nbt_data;
    if (!melon::util::gunzip(gz_buffer, &nbt_data))
    {
        end = std::chrono::high_resolution_clock::now();
        std::cout << "Successfully decompressed NBT data (" << nbt_data.size() << " bytes)." << std::endl;
        std::cout << "Elapsed time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start) << std::endl;
    }
    else
    {
        return 1;
    }

    std::vector<melon::nbt::compound> parsed_nbt;
    parsed_nbt.reserve(10000);

    try
    {
        start = std::chrono::high_resolution_clock::now();

        for (int index = 0; index < 10000; index++)
            parsed_nbt.emplace_back(&nbt_data);

        end = std::chrono::high_resolution_clock::now();
        std::cout << "Successfully parsed NBT data." << std::endl;

        std::cout << "Elapsed time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start) << std::endl;
    }
    catch (std::exception &e)
    {
        std::cerr << "Failed to parse NBT Data: " << e.what() << std::endl;
    }

    parsed_nbt.clear();
    std::cout << "Deleted parsed NBT." << std::endl;

    return 0;
}
