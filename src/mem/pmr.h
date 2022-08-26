//
// Created by MrGrim on 8/25/2022.
//

#ifndef MELON_MEM_PMR_H
#define MELON_MEM_PMR_H

#include <memory>
#include <memory_resource>

namespace melon::mem::pmr {

    class debug_monotonic_buffer_resource : public std::pmr::monotonic_buffer_resource
    {
    public:
        debug_monotonic_buffer_resource(void *buffer, std::size_t buffer_size);
        ~debug_monotonic_buffer_resource() override;

    protected:
        void *do_allocate(std::size_t bytes, std::size_t alignment) override;
        void do_deallocate(void *p, std::size_t bytes, std::size_t alignment) override;
        [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override;

    private:
        int64_t total_bytes_allocated   = 0;
        int64_t total_bytes_deallocated = 0;
    };

}

#endif //MELON_MEM_PMR_H
