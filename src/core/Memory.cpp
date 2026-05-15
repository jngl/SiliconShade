//
// Created by jngl on 13/04/2026.
//
#include <retro3d/core/Memory.h>

#include <cstdlib>

SystemMemory::SystemMemory(uint64_t p_size)
{
    // Ensure size is a multiple of alignment for std::aligned_alloc
    uint64_t alignment = 32;
    uint64_t aligned_size = (p_size + alignment - 1) & ~(alignment - 1);
    data = static_cast<std::byte*>(std::aligned_alloc(alignment, aligned_size));
    size = aligned_size;
}

SystemMemory::~SystemMemory()
{
    std::free(data);
}
