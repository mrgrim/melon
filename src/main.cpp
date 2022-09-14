#include <iostream>
#include <vector>
#include <cstring>
#include <memory>
#include <memory_resource>
#include <chrono>
#include <span>
#include <deque>
#include <set>

#include "util/deflate.h"
#include "util/file.h"
#include "mem/pmr.h"
#include "nbt/compound.h"
#include "nbt/list.h"

using namespace melon;

#define NBT_DEBUG true

int main()
{
    std::vector<char> gz_buffer;

    auto start = std::chrono::high_resolution_clock::now();

    if (melon::util::file_to_vec(R"(E:/Games/Minecraft/Servers/Fabric/World/level.dat)", gz_buffer))
    {
        std::cerr << "Unable to load level.dat: " << strerror(errno) << std::endl;
        return 1;
    }

    auto end = std::chrono::high_resolution_clock::now();

    std::cout << "Successfully read file off disk." << std::endl;
    std::cout << "Elapsed time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start) << std::endl;

    start = std::chrono::high_resolution_clock::now();

    auto [nbt_data_ptr, nbt_data_size] = util::gzip_inflate(gz_buffer);

    end = std::chrono::high_resolution_clock::now();
    std::cout << "Successfully decompressed NBT data (" << nbt_data_size << " bytes)." << std::endl;
    std::cout << "Elapsed time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start) << std::endl;

#if NBT_DEBUG == true
    nbt::compound *parsed_nbt;

    auto                      pmr_buf   = malloc(40000);
    //std::pmr::memory_resource *pmr_rsrc = new melon::mem::pmr::debug_monotonic_buffer_resource(pmr_buf, 40000);
    std::pmr::memory_resource *pmr_rsrc = new std::pmr::monotonic_buffer_resource(pmr_buf, 40000);
#else
    auto result_alloc = new std::pmr::monotonic_buffer_resource(sizeof(nbt::compound) * 50000, std::pmr::get_default_resource());

    nbt::compound *parsed_nbt[50000];
    std::unique_ptr<char[]> nbt_data_copies[50000];

    auto pmr_buf = malloc(40000l * 50000l);
    auto pmr_rsrc = new std::pmr::monotonic_buffer_resource(pmr_buf, 40000l * 50000l);

    for (int index = 0; index < 50000; index++)
    {
        nbt_data_copies[index] = std::make_unique<char[]>(nbt_data_size);
        std::memcpy(nbt_data_copies[index].get(), nbt_data_ptr.get(), nbt_data_size);
        auto ptr = result_alloc->allocate(sizeof(nbt::compound), alignof(nbt::compound));
        parsed_nbt[index] = static_cast<nbt::compound *>(ptr);
    }
#endif

    try
    {
        start = std::chrono::high_resolution_clock::now();

#if NBT_DEBUG == true
        parsed_nbt = new nbt::compound(std::move(nbt_data_ptr), nbt_data_size, std::unique_ptr<std::pmr::memory_resource>(pmr_rsrc));
#else
        for (int index = 0; index < 50000; index++)
        {
            new(parsed_nbt[index]) nbt::compound(std::move(nbt_data_copies[index]), nbt_data_size, pmr_rsrc);
        }
#endif

        end = std::chrono::high_resolution_clock::now();
        std::cout << "Successfully parsed NBT data." << std::endl;

        std::cout << "Elapsed time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start) << std::endl;

#if NBT_DEBUG == true
        parsed_nbt->put<nbt::tag_float>("Test Value", 1.123f);
        parsed_nbt->put<nbt::tag_long>("Test Value 2", (int64_t)34198);

        parsed_nbt->put<nbt::tag_float>("Test Value", 8.438f);

        parsed_nbt->put<nbt::tag_int_array>("Test Int Deduced Initializer Array", { 1, 2, 3, 4, 5, 6 });
        parsed_nbt->put<nbt::tag_long_array>("Test Long Explicit Initializer Array", std::initializer_list<int64_t>{ 7, 8, 9, 10, 11, 12 });
        parsed_nbt->put<nbt::tag_byte_array>("Test Byte Vector Array", std::vector<int8_t>{ 13, 14, 15, 16, 17, 18 });
        parsed_nbt->put<nbt::tag_int_array>("Test Int Array Array", std::array<int32_t, 6>{ 19, 20, 21, 22, 23, 24 });
        parsed_nbt->put<nbt::tag_long_array>("Test Long Deque Array", std::deque<int64_t>{ 25, 26, 27, 28, 29, 30 });
        parsed_nbt->put<nbt::tag_byte_array>("Test Byte Set Array", std::set<int8_t>{ -1, -2, -3, -4, -5, -6 });

        auto end_gateway_list = parsed_nbt->get<nbt::tag_compound>("Data").value()
                                          ->get<nbt::tag_compound>("DragonFight").value()
                                          ->get<nbt::tag_list>("Gateways").value();

        for (auto &gateway: nbt::list::range<nbt::tag_int>{ end_gateway_list })
            std::cout << gateway++ << " ";

        std::cout << std::endl;

        auto enabled_datapacks = parsed_nbt->get<nbt::tag_compound>("Data").value()
                                           ->get<nbt::tag_compound>("DataPacks").value()
                                           ->get<nbt::tag_list>("Enabled").value();

        for (const auto pack_name: nbt::list::range<nbt::tag_string>{ enabled_datapacks })
            std::cout << pack_name << std::endl;

        auto scheduled_events = parsed_nbt->get<nbt::tag_compound>("Data").value()
                                          ->get<nbt::tag_list>("ScheduledEvents").value();

        auto event_time = parsed_nbt->get<nbt::tag_compound>("Data").value()
                                    ->get<nbt::tag_list>("ScheduledEvents").value()
                                    ->at<nbt::tag_compound>(1)
                                    ->get<nbt::tag_long>("TriggerTime").value();

        //for (const auto event : nbt::list::range<nbt::tag_compound>{ scheduled_events })

        std::cout << end_gateway_list->at<nbt::tag_int>(2) << " " << enabled_datapacks->at<nbt::tag_string>(3) << " "
                  << scheduled_events->at<nbt::tag_compound>(1)->get<nbt::tag_long>("TriggerTime").value() << " " << event_time << std::endl;

        try
        {
            parsed_nbt->put<nbt::tag_short>("Test Value 2", (int16_t)498); // Should catch attempt to write different type to existing tag
        }
        catch (std::exception &e)
        {
            std::cerr << "Caught Exception: " << e.what() << std::endl;
        }

        std::string debug_out;
        parsed_nbt->to_snbt(debug_out);
        std::cout << debug_out << std::endl;

        std::cout << "sizeof(void *): " << sizeof(void *) << std::endl;

        delete parsed_nbt;
        std::cout << "Deleted parsed NBT." << std::endl;

#else
        std::srand(std::time(0));
        std::string debug_out;
        parsed_nbt[std::rand() % 50000]->to_snbt(debug_out);
        std::cout << debug_out << std::endl;
#endif
    }
    catch (std::exception &e)
    {
        std::cerr << "Failed to parse NBT Data: " << e.what() << std::endl;
    }

#if NBT_DEBUG == false
    delete result_alloc;
#endif

    free(pmr_buf);

    return 0;
}
