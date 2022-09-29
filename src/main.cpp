#include <iostream>
#include <vector>
#include <cstring>
#include <memory>
#include <memory_resource>
#include <chrono>
#include <span>
#include <deque>
#include <set>
#include <cstdlib>

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

    std::srand(std::time(nullptr));
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

//    try
//    {
    start = std::chrono::high_resolution_clock::now();

#if NBT_DEBUG == true
    auto parsed_nbt = nbt::compound(std::move(nbt_data_ptr), nbt_data_size);
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
    std::cout << "Calculated level.dat size: " << parsed_nbt.size() << std::endl;

    parsed_nbt.insert<nbt::tag_float>("Test Value", 1.123f);
    parsed_nbt.insert<nbt::tag_long>("Test Value 2", (int64_t)34198);

    parsed_nbt.insert<nbt::tag_float>("Test Value", 8.438f, { .overwrite = true });

    parsed_nbt.insert<nbt::tag_int_array>("Test Int Deduced Initializer Array", { 1, 2, 3, 4, 5, 6 });
    parsed_nbt.insert<nbt::tag_long_array>("Test Long Explicit Initializer Array", std::initializer_list<int64_t>{ 7, 8, 9, 10, 11, 12 });
    parsed_nbt.insert<nbt::tag_byte_array>("Test Byte Vector Array", std::vector<int8_t>{ 13, 14, 15, 16, 17, 18 });
    parsed_nbt.insert<nbt::tag_int_array>("Test Int Array Array", std::array<int32_t, 6>{ 19, 20, 21, 22, 23, 24 });
    parsed_nbt.insert<nbt::tag_long_array>("Test Long Deque Array", std::deque<int64_t>{ 25, 26, 27, 28, 29, 30 });
    parsed_nbt.insert<nbt::tag_byte_array>("Test Byte Set Array", std::set<int8_t>{ -1, -2, -3, -4, -5, -6 });

    auto &level_compound = parsed_nbt.find<nbt::tag_compound>("Data").value().get();

    auto end_gateway_list_opt = parsed_nbt.find<nbt::tag_compound>("Data")
                                          .find<nbt::tag_compound>("DragonFight")
                                          .find<nbt::tag_list>("Gateways");

    if (end_gateway_list_opt)
        for (nbt::list &end_gateway_list = *end_gateway_list_opt; auto &gateway: nbt::list::range<nbt::tag_int>{ end_gateway_list })
            std::cout << gateway++ << " ";

    std::cout << std::endl;

    auto &enabled_datapacks = parsed_nbt.find<nbt::tag_compound>("Data")
                                        .find<nbt::tag_compound>("DataPacks")
                                        .find<nbt::tag_list>("Enabled").value().get();

    for (const auto pack_name: nbt::list::range<nbt::tag_string>{ enabled_datapacks })
        std::cout << pack_name << std::endl;

    auto my_compound = nbt::compound("");
    my_compound.create<nbt::tag_compound>("Test Compound", [](nbt::compound &test_compound) {
        test_compound.insert<nbt::tag_string>("Test String", "This is a test string!");
        test_compound.insert<nbt::tag_float>("Test Float", 78.2945f);

        test_compound.create<nbt::tag_list>("Test Array List", nbt::tag_byte_array, [](nbt::list &test_list) {
            test_list.reserve(2);

            test_list.push<nbt::tag_byte_array>({ 1, 2, 3, 4, 5 });
            test_list.push<nbt::tag_byte_array>(std::array<int8_t, 7>{ 23, 76, 63, 25, 36, 34, 87 });
        });

        test_compound.create<nbt::tag_list>("Test String List", nbt::tag_string, [](nbt::list &string_list) {
            string_list.reserve(3);

            string_list.push<nbt::tag_string>("Test String 1");
            string_list.push<nbt::tag_string>("Test String 2");
            string_list.push<nbt::tag_string>("Test String 3");
        });

        test_compound.create<nbt::tag_list>("Test Primitive List", nbt::tag_float, [](nbt::list &float_list) {
            float_list.reserve(6);

            float_list.push<nbt::tag_float>(2567.643f);
            float_list.push<nbt::tag_float>(34.55462f);
            float_list.push<nbt::tag_float>(345.6f);
            float_list.push<nbt::tag_float>(543662.436f);
            float_list.push<nbt::tag_float>(340.98657f);
            float_list.push<nbt::tag_float>(14.83567f);
        });

        test_compound.create<nbt::tag_list>("Test List of Lists", nbt::tag_list, [](nbt::list &list_list) {
            list_list.push<nbt::tag_list>(nbt::tag_double, [](nbt::list &list) {
                list.push<nbt::tag_double>(5646.34367456);
                list.push<nbt::tag_double>(34565.43654);
                list.push<nbt::tag_double>(354.34543656754677);
            });

            list_list.push<nbt::tag_list>(nbt::tag_short, [](nbt::list &list) {
                list.push<nbt::tag_short>((int16_t)-671);
                list.push<nbt::tag_short>((int16_t)15783);
                list.push<nbt::tag_short>((int16_t)42);
            });
        });

        std::array<std::string_view, 4> names = { "Foo 1", "Foo 2", "Foo 3", "Foo 4" };
        test_compound.create<nbt::tag_list>("Test Compound List", nbt::tag_compound, [names](nbt::list &name_list) {
            name_list.reserve(names.size());

            for (auto &name: names)
            {
                name_list.push<nbt::tag_compound>([name](nbt::compound &name_compound) {
                    name_compound.insert<nbt::tag_string>("Name", name);
                    name_compound.insert<nbt::tag_short>("Rand", static_cast<int16_t>(std::rand() % std::numeric_limits<int16_t>::max()));
                });
            }
        });
    });

    std::cout << *(my_compound.to_snbt()) << std::endl;
    std::cout << "my_compound size: " << my_compound.size() << std::endl;

    {
        auto &test_compound = my_compound.find<nbt::tag_compound>("Test Compound").value().get();

        for (auto itr = test_compound.begin(); itr != test_compound.end();)
        {
            auto &&[name, tag_type, _] = *itr;
            std::cout << "Iterated over tag \"" << name << "\" of type " << +tag_type << "." << std::endl;

            if (name == "Test String List")
            {
                std::cout << "Erasing \"" << name << "\"." << std::endl;
                itr = test_compound.erase(std::move(itr));
            }
            else
                itr++;
        }

        if (test_compound.erase("Test String") > 0)
        {
            std::cout << "Find \"Test String\" by name and successfully deleted." << std::endl;
        }
    }

    if (const auto &comp_result = my_compound.find("Test Compound", nbt::tag_compound))
    {
        auto &[ckey, ctype, ctag] = comp_result.value();
        auto &test_compound       = std::get<nbt::tag_compound>(ctag).get();

        if (const auto &float_result = test_compound.find("Test Float"))
        {
            auto &[fkey, ftype, ftag] = float_result.value();

            if (ftype == nbt::tag_float)
            {
                auto &fvalue = std::get<nbt::tag_float>(ftag).get();

                std::cout << "Found float value of \"" << fvalue << "\" with generic search. Changing to \"" << (1.0 / 137.0) << "\"." << std::endl;
                fvalue = 1.0 / 137.0;
            }
        }
    }

    std::cout << *(my_compound.to_snbt()) << std::endl;
    std::cout << "my_compound size: " << my_compound.size() << std::endl;

    auto &scheduled_events = parsed_nbt.find<nbt::tag_compound>("Data")
                                       .find<nbt::tag_list>("ScheduledEvents").value().get();

    for (auto &&event: nbt::list::range<nbt::tag_compound>{ scheduled_events })
        std::cout << "Scheduled Event " << event.find<nbt::tag_string>("Name").value() << " at " << event.find<nbt::tag_long>("TriggerTime").value() << std::endl;

    std::cout << "And a random repeat..." << std::endl;
    auto &event = scheduled_events.at<nbt::tag_compound>(std::rand() % scheduled_events.count);
    std::cout << "Scheduled Event " << event.find<nbt::tag_string>("Name").value() << " at " << event.find<nbt::tag_long>("TriggerTime").value() << std::endl;

    auto &end_gateway_list = (*end_gateway_list_opt).get();
    std::cout << end_gateway_list.at<nbt::tag_int>(2) << " " << enabled_datapacks.at<nbt::tag_string>(3) << " "
              << scheduled_events.at<nbt::tag_compound>(1).find<nbt::tag_long>("TriggerTime").value() << std::endl;

    auto trader_ints = level_compound.find<nbt::tag_int_array>("WanderingTraderId").value();

    std::cout << "Wandering Trader ID (" << trader_ints.size() << "): ";
    for (auto &id: trader_ints)
        std::cout << id << " ";
    std::cout << std::endl;

    try
    {
        parsed_nbt.insert<nbt::tag_short>("Test Value 2", (int16_t)498, { .overwrite = true }); // Should catch attempt to write different type to existing tag
    }
    catch (std::exception &e)
    {
        std::cerr << "Caught Exception: " << e.what() << std::endl;
    }

    std::string debug_out;

    parsed_nbt.to_snbt(debug_out);
    std::cout << debug_out << std::endl;

    std::cout << "sizeof(void *): " << sizeof(void *) << std::endl;

    std::cout << "Modified level.dat size prior to deletion: " << parsed_nbt.size() << std::endl;
    //delete parsed_nbt;
    //std::cout << "Deleted parsed NBT." << std::endl;

#else
    std::srand(std::time(0));
    std::string debug_out;
    parsed_nbt[std::rand() % 50000]->to_snbt(debug_out);
    std::cout << debug_out << std::endl;
#endif
/*    }
    catch (std::exception &e)
    {
        std::cerr << "Failed to parse NBT Data: " << e.what() << std::endl;
    }*/

#if NBT_DEBUG == false
    delete result_alloc;
#endif

    delete (pmr_rsrc);
    free(pmr_buf);

    return 0;
}
