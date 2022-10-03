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
    struct default_deleter
    {
        std::pmr::memory_resource *pmr_rsrc;

        default_deleter() = delete;

        explicit default_deleter(std::pmr::memory_resource *pmr_rsrc_in) : pmr_rsrc(pmr_rsrc_in)
        { }

        template<class U>
        requires std::is_convertible_v<U *, T *>
        explicit default_deleter(const default_deleter<U> &in) noexcept :pmr_rsrc(in.pmr_rsrc)
        { }

        void operator()(T *ptr) const
        {
            std::destroy_at(ptr);
            pmr_rsrc->deallocate(ptr, sizeof(T), alignof(T));
        }
    };

    template<class T>
    struct array_deleter
    {
        std::pmr::memory_resource *pmr_rsrc;
        size_t size;

        array_deleter() = delete;

        explicit array_deleter(std::pmr::memory_resource *pmr_rsrc_in, size_t size_in) : pmr_rsrc(pmr_rsrc_in), size(size_in)
        { }

        template<class U>
        requires std::is_convertible_v<U *, T *>
        explicit array_deleter(const array_deleter<U> &in) noexcept :pmr_rsrc(in.pmr_rsrc), size(in.size)
        { }

        void operator()(std::decay_t<T> ptr) const
        {
            std::destroy_at(ptr);
            pmr_rsrc->deallocate(ptr, sizeof(std::remove_pointer_t<std::decay_t<T>>) * size, alignof(T));
        }
    };

    template<class T>
    struct generic_deleter
    {
        std::pmr::memory_resource *pmr_rsrc;
        size_t size;
        size_t align;

        generic_deleter() = delete;

        explicit generic_deleter(std::pmr::memory_resource *pmr_rsrc_in, size_t size_in, size_t align_in) : pmr_rsrc(pmr_rsrc_in), size(size_in), align(align_in)
        { }

        template<class U>
        requires std::is_convertible_v<U *, T *>
        explicit generic_deleter(const generic_deleter<U> &in) noexcept :pmr_rsrc(in.pmr_rsrc), size(in.size), align(in.align)
        { }

        void operator()(T ptr) const
        {
            std::destroy_at(ptr);
            pmr_rsrc->deallocate(ptr, size, align);
        }
    };

    template<class T>
    using unique_ptr = std::unique_ptr<T, default_deleter<T>>;

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

    template<class T>
    auto destroy_obj_using_pmr(std::pmr::memory_resource *pmr_rsrc, T *obj)
    requires (!std::is_array_v<T>)
    {
        if constexpr (std::is_destructible_v<T>)
            std::destroy_at<T>(obj);

        pmr_rsrc->deallocate(obj,sizeof(T), alignof(T));
    }

    template<class T, class B = std::remove_pointer_t<std::decay_t<T>>>
    requires (std::is_array_v<T>)
    auto make_unique(std::pmr::memory_resource *pmr_rsrc, size_t size)
    {
        return std::unique_ptr<T, array_deleter<T>>(static_cast<B *>(pmr_rsrc->allocate(sizeof(B) * size, alignof(B))), array_deleter<T>(pmr_rsrc, size));
    }

    template<class T, class... Args>
    requires (!std::is_array_v<T>)
    auto make_unique(std::pmr::memory_resource *pmr_rsrc, Args &&... args)
    {
            return std::unique_ptr<T, default_deleter<T>>(make_obj_using_pmr<T>(pmr_rsrc, std::forward<Args>(args)...), default_deleter<T>(pmr_rsrc));
    }

    template<class T>
    auto make_empty_unique(std::pmr::memory_resource *pmr_rsrc)
    {
        return std::unique_ptr<T, default_deleter<T>>(nullptr, default_deleter<T>(pmr_rsrc));
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
        explicit recording_mem_resource(std::pmr::memory_resource *upstream_resource_in, bool start_recording = false, size_t initial_size = 512) noexcept
                : upstream_resource(upstream_resource_in),
                  alloc_records(),
                  dealloc_records(),
                  recording(start_recording)
        {
            alloc_records.reserve(initial_size);
            dealloc_records.reserve(initial_size);
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

        auto &get_records() { return alloc_records; }

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

                    for (auto rec_itr = dealloc_records.begin(); rec_itr != dealloc_records.end(); rec_itr++)
                    {
                        if (rec_itr->ptr == p && rec_itr->bytes == bytes && rec_itr->alignment == alignment)
                        {
                            found = true;
                            std::cout << "Double de-allocation detected!" << std::endl;
                            break;
                        }

                        if (rec_itr->ptr == p)
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
            }

            upstream_resource->deallocate(p, bytes, alignment);
        }

        [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override
        {
            auto other_cast = dynamic_cast<const recording_mem_resource *>(&other);
            auto other_upstream = other_cast ? other_cast->upstream_resource : nullptr;

            return (this == &other || upstream_resource == other_upstream);
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
        std::vector<alloc_rec>    dealloc_records;
        bool                      recording;
    };
}

#endif //MELON_MEM_PMR_H
