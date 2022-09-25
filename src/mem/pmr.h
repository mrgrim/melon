//
// Created by MrGrim on 8/25/2022.
//

#ifndef MELON_MEM_PMR_H
#define MELON_MEM_PMR_H

#include <memory>
#include <memory_resource>
#include <ranges>
#include <vector>

namespace melon::mem::pmr
{
    template<class T>
    struct default_pmr_deleter
    {
        std::pmr::memory_resource *pmr_rsrc;

        default_pmr_deleter() = delete;

        explicit default_pmr_deleter(std::pmr::memory_resource *pmr_rsrc_in) : pmr_rsrc(pmr_rsrc_in)
        { }

        template<class U>
        requires std::is_convertible_v<U *, T *>
        explicit default_pmr_deleter(const default_pmr_deleter<U> &in) noexcept :pmr_rsrc(in.pmr_rsrc)
        { }

        void operator()(T *ptr) const
        {
            std::destroy_at(ptr);
            pmr_rsrc->deallocate(ptr, sizeof(T), alignof(T));
        }
    };

    template<class T>
    using unique_ptr = std::unique_ptr<T, default_pmr_deleter<T>>;

    template<class T, class... Args>
    auto make_obj_using_pmr(std::pmr::memory_resource *pmr_rsrc, Args &&... args)
    requires (!std::is_array_v<T>)
    {
        auto ptr = static_cast<T *>(pmr_rsrc->allocate(sizeof(T), alignof(T)));

        try
        {
            if constexpr (std::is_constructible_v<T, Args..., std::pmr::memory_resource *>)
                return static_cast<T *>(::new(ptr) T(std::forward<Args>(args)..., pmr_rsrc));
            else
                return static_cast<T *>(::new(ptr) T(std::forward<Args>(args)...));
        }
        catch (...)
        {
            pmr_rsrc->deallocate(ptr, sizeof(T), alignof(T));
            throw;
        }
    }

    template<class T, class... Args>
    auto make_pmr_unique(std::pmr::memory_resource *pmr_rsrc, Args &&... args)
    {
        return std::unique_ptr<T, default_pmr_deleter<T>>(make_obj_using_pmr<T>(pmr_rsrc, std::forward<Args>(args)...), default_pmr_deleter<T>(pmr_rsrc));
    }

    template<class T, class... Args>
    auto make_pmr_empty_unique(std::pmr::memory_resource *pmr_rsrc)
    {
        return std::unique_ptr<T, default_pmr_deleter<T>>(nullptr, default_pmr_deleter<T>(pmr_rsrc));
    }

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


    class recording_mem_resource : public std::pmr::memory_resource
    {
    public:
        explicit recording_mem_resource(std::pmr::memory_resource *upstream_resource_in, size_t initial_size = 512) noexcept
                : upstream_resource(upstream_resource_in),
                  alloc_records(initial_size),
                  recording(false)
        { }

        ~recording_mem_resource() override
        {
            delete upstream_resource;
        }

        void start_recording()
        { recording = true; }

        void stop_recording()
        {
            recording = false;
            alloc_records.clear();
        }

        void deallocate_recorded()
        {
            // Must go in reverse order as do_deallocate modifies the vector
            for (auto & alloc_record : alloc_records)
                do_unchecked_deallocate(alloc_record.ptr, alloc_record.bytes, alloc_record.alignment);

            alloc_records.clear();
        }

    protected:
        void *do_allocate(std::size_t bytes, std::size_t alignment) override
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
        void do_deallocate(void *p, std::size_t bytes, std::size_t alignment) override
        {
            if (recording)
            {
                auto last_rec = alloc_records.back();

                if (last_rec.ptr != p || last_rec.bytes != bytes || last_rec.alignment != alignment)
                {
                    for (auto rec_itr = alloc_records.begin(); rec_itr != alloc_records.end(); rec_itr++)
                        if (rec_itr->ptr == p && rec_itr->bytes == bytes && rec_itr->alignment == alignment)
                        {
                            alloc_records.erase(rec_itr);
                            break;
                        }
                }
                else
                    alloc_records.pop_back();
            }

            upstream_resource->deallocate(p, bytes, alignment);
        }

        [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override
        {
            return (this == &other || upstream_resource == &other);
        }

    private:
        struct alloc_rec
        {
            void        *ptr;
            std::size_t bytes;
            std::size_t alignment;
        };

        void do_unchecked_deallocate(void *p, std::size_t bytes, std::size_t alignment)
        {
            upstream_resource->deallocate(p, bytes, alignment);
        }

        std::pmr::memory_resource *upstream_resource;
        std::vector<alloc_rec>    alloc_records;
        bool                      recording;
    };
}

#endif //MELON_MEM_PMR_H
