#include <iostream>
#include <vector>
#include <cstring>
#include <memory>
#include <chrono>

#include "util/deflate.h"
#include "util/file.h"
#include "nbt/compound.h"

//#define NBT_DEBUG true

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

    std::vector<std::byte> nbt_data;
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

#if NBT_DEBUG == true
    melon::nbt::compound *parsed_nbt;
    auto *pmr_buf = new std::byte[30000];

    melon::nbt::compound::compounds_parsed = 0;
    melon::nbt::compound::lists_parsed = 0;
    melon::nbt::compound::primitives_parsed = 0;
    melon::nbt::compound::strings_parsed = 0;
    melon::nbt::compound::arrays_parsed = 0;
#else
    std::vector<melon::nbt::compound> parsed_nbt;
    parsed_nbt.reserve(10000);

    std::vector<std::unique_ptr<std::vector<std::byte>>> nbt_data_copies;
    nbt_data_copies.reserve(10000);
    auto pmr_buffers = std::array<std::byte *, 10000>();

    for (int index = 0; index < 10000; index++)
    {
        nbt_data_copies.push_back(std::make_unique<std::vector<std::byte>>(nbt_data));
        pmr_buffers[index] = new std::byte[30000];
    }

#endif

    try
    {
        start = std::chrono::high_resolution_clock::now();

#if NBT_DEBUG == true
        parsed_nbt = new melon::nbt::compound(std::move(std::make_unique<std::vector<std::byte>>(nbt_data)), pmr_buf, 30000);
#else
        for (int index = 0; index < 10000; index++)
            parsed_nbt.emplace_back(std::move(nbt_data_copies[index]), pmr_buffers[index], 30000);
#endif

        end = std::chrono::high_resolution_clock::now();
        std::cout << "Successfully parsed NBT data." << std::endl;

        std::cout << "Elapsed time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start) << std::endl;
    }
    catch (std::exception &e)
    {
        std::cerr << "Failed to parse NBT Data: " << e.what() << std::endl;
    }

#if NBT_DEBUG == true
    delete parsed_nbt;
#else
    parsed_nbt.clear();

    for (int index = 0; index < 10000; index++)
        delete pmr_buffers[index];
#endif

    std::cout << "Deleted parsed NBT." << std::endl;

    return 0;
}
