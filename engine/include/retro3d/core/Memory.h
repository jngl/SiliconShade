//
// Created by jngl on 13/04/2026.
//

#ifndef INC_3D_TEST_MEMORY_H
#define INC_3D_TEST_MEMORY_H

#include <retro3d/core/Math.h>

#include <cassert>
#include <cstring>
#include <type_traits>
#include <new>

template<class T>
struct View{
    T* data = nullptr;
    uint64_t size = 0;
} ;

struct SystemMemory: View<std::byte>
{
    explicit SystemMemory(uint64_t size);
    ~SystemMemory();
};

class Arena{
public:
    explicit constexpr Arena(const View<std::byte> memory) :
        memory_bloc{memory}
    {
    }

    constexpr void clear()
    {
        current = 0;
    }

    template<class T>
    View<T> allocate(uint64_t nb_object, uint64_t alignment = alignof(T))
    {
        static_assert(std::is_trivially_destructible_v<T>, "Arena only supports trivially destructible types");

        uint64_t size_in_bytes = nb_object * sizeof(T);
        View<std::byte> raw = allocateImpl(size_in_bytes, alignment);
        
        T* data = reinterpret_cast<T*>(raw.data);
        if constexpr (!std::is_trivially_default_constructible_v<T>) {
            for (uint64_t i = 0; i < nb_object; ++i) {
                new (&data[i]) T();
            }
        } else {
            // For trivially constructible types, allocateImpl already zeroed the memory
        }

        return View<T>{ data, nb_object };
    }

private:
    View<std::byte> memory_bloc;
    uint64_t current = 0;

    View<std::byte> allocateImpl(const uint64_t size, uint64_t alignment)
    {
        uintptr_t raw_addr = reinterpret_cast<uintptr_t>(memory_bloc.data + current);
        uintptr_t aligned_addr = (raw_addr + alignment - 1) & ~(alignment - 1);
        uint64_t padding = aligned_addr - raw_addr;

        assert(current + padding + size <= memory_bloc.size);

        std::byte* data = reinterpret_cast<std::byte*>(aligned_addr);
        current += padding + size;

        std::memset(data, 0, size);
        return { data, size };
    }
} ;

#endif //INC_3D_TEST_MEMORY_H
