//
// Created by MrGrim on 8/25/2022.
//

#include <iostream>
#include "pmr.h"

namespace melon::mem::pmr
{
    void recording_mem_resource::start_recording()
    {
        recording = true;
    }

    void recording_mem_resource::stop_recording()
    {
        recording = false;
        alloc_records.clear();
    }

    void recording_mem_resource::deallocate_recorded()
    {
        // Must go in reverse order as do_deallocate modifies the vector
        for (auto & alloc_record : alloc_records)
            do_unchecked_deallocate(alloc_record.ptr, alloc_record.bytes, alloc_record.alignment);

        alloc_records.clear();
    }

    void *recording_mem_resource::do_allocate(std::size_t bytes, std::size_t alignment)
    {
        auto ptr = upstream_resource->allocate(bytes, alignment);

        if (recording)
        {
            try
            {
                alloc_records.push_back({ ptr, bytes, alignment });
            }
            catch (...)
            {
                upstream_resource->deallocate(ptr, bytes, alignment);
                throw;
            }
        }

        return ptr;
    }

    // This is designed to deallocate in reverse order of allocation. Random deallocations force a full vector search.
    void recording_mem_resource::do_deallocate(void *p, std::size_t bytes, std::size_t alignment)
    {
        if (recording)
        {
            auto last_rec = alloc_records.back();

            if (last_rec.ptr != p || last_rec.bytes != bytes || last_rec.alignment != alignment)
            {
                bool found = false;

                for (auto rec_itr = alloc_records.begin(); rec_itr != alloc_records.end(); rec_itr++)
                {
                    if (rec_itr->ptr == p && rec_itr->bytes == bytes && rec_itr->alignment == alignment)
                    {
                        alloc_records.erase(rec_itr);
                        found = true;
                        break;
                    }

                    if (rec_itr->ptr == p)
                    {
                        found = true;
                        std::cout << "De-allocation with mismatched parameters!" << std::endl;
                    }
                }

                for (auto & dealloc_record : dealloc_records)
                {
                    if (dealloc_record.ptr == p && dealloc_record.bytes == bytes && dealloc_record.alignment == alignment)
                    {
                        found = true;
                        std::cout << "Double de-allocation detected!" << std::endl;
                        break;
                    }

                    if (dealloc_record.ptr == p)
                    {
                        found = true;
                        std::cout << "Double de-allocation with mismatched parameters!" << std::endl;
                    }
                }

                if (!found)
                    std::cout << "De-allocation without record!" << std::endl;
            }
            else
                alloc_records.pop_back();

            dealloc_records.push_back({ p, bytes, alignment });
        }

        upstream_resource->deallocate(p, bytes, alignment);
    }
}