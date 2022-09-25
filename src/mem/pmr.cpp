//
// Created by MrGrim on 8/25/2022.
//

#include <iostream>
#include "pmr.h"

namespace melon::mem::pmr
{

    void *debug_monotonic_buffer_resource::do_allocate(std::size_t bytes, std::size_t alignment)
    {
        std::cout << "Allocated " << bytes << " bytes.\n";
        total_bytes_allocated += bytes;
        return monotonic_buffer_resource::do_allocate(bytes, alignment);
    }

    void debug_monotonic_buffer_resource::do_deallocate(void *p, std::size_t bytes, std::size_t alignment)
    {
        std::cout << "De-allocated " << bytes << " bytes (no-op).\n";
        total_bytes_deallocated += bytes;
        monotonic_buffer_resource::do_deallocate(p, bytes, alignment);
    }

    bool debug_monotonic_buffer_resource::do_is_equal(const std::pmr::memory_resource &other) const noexcept
    {
        std::cout << "Compared memory resources\n";
        return monotonic_buffer_resource::do_is_equal(other);
    }

    debug_monotonic_buffer_resource::debug_monotonic_buffer_resource(void *buffer, std::size_t buffer_size) : monotonic_buffer_resource(buffer, buffer_size)
    {
        std::cout << "Created memory resources\n";
    }

    debug_monotonic_buffer_resource::~debug_monotonic_buffer_resource()
    {
        std::cout << "Destroyed memory resources. Total bytes allocated: " << total_bytes_allocated << ", Total bytes deallocated: " << total_bytes_deallocated << "\n";
        monotonic_buffer_resource::~monotonic_buffer_resource();
    }
}