//
// Created by MrGrim on 8/25/2022.
//

#ifndef MELON_MEM_PMR_H
#define MELON_MEM_PMR_H

#include <memory>
#include <memory_resource>

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

        void start_recording();
        void stop_recording();
        void deallocate_recorded();

        auto &get_alloc_records() { return alloc_records; }
        auto &get_dealloc_records() { return dealloc_records; }

    protected:
        void *do_allocate(std::size_t bytes, std::size_t alignment) override;

        // This is designed to deallocate in reverse order of allocation. Random deallocations force a full vector search.
        void do_deallocate(void *p, std::size_t bytes, std::size_t alignment) override;

        [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override
        { return (this == &other); }

    private:
        struct alloc_rec
        {
            void        *ptr;
            std::size_t bytes;
            std::size_t alignment;
        };

        void do_unchecked_deallocate(void *p, std::size_t bytes, std::size_t alignment)
        { upstream_resource->deallocate(p, bytes, alignment); }

        std::pmr::memory_resource *upstream_resource;
        std::vector<alloc_rec>    alloc_records;
        std::vector<alloc_rec>    dealloc_records;
        bool                      recording;
    };
}

#endif //MELON_MEM_PMR_H
