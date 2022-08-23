//
// Created by MrGrim on 8/16/2022.
//

#include <cstdlib>
#include <memory>
#include <memory_resource>

#include "nbt/nbt.h"

namespace melon::nbt
{
    primitive_tag::primitive_tag(primitive_tag&& in) noexcept // NOLINT(cppcoreguidelines-pro-type-member-init)
    {
        tag_type = in.tag_type;
        size = in.size;
        value.tag_long = in.value.tag_long;

        in.tag_type = tag_end;
        in.size = 0;
        in.value.tag_long = 0;
    }

    primitive_tag& primitive_tag::operator=(primitive_tag&& in) noexcept
    {
        if (this != &in)
        {
//            if (tag_properties[tag_type].is_complex && (void *)(value.tag_byte_array) != nullptr)
//                free((void *)(value.tag_byte_array));

            tag_type = in.tag_type;
            size = in.size;
            value.tag_long = in.value.tag_long;

            in.tag_type = tag_end;
            in.size = 0;
            in.value.tag_long = 0;
        }

        return *this;
    }

    primitive_tag::~primitive_tag()
    {
#if DEBUG == true
        std::cout << "Deleting primitive." << std::endl;
#endif
//        if (tag_properties[tag_type].is_complex && (void *)(value.tag_string) != nullptr)
//            free((void *)(value.tag_byte_array));
    }

    std::variant<std::pmr::memory_resource *, std::shared_ptr<std::pmr::memory_resource>> get_std_default_pmr_rsrc()
    {
        std::variant<std::pmr::memory_resource *, std::shared_ptr<std::pmr::memory_resource>> ret;
        ret = std::pmr::get_default_resource();
        return ret;
    }

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