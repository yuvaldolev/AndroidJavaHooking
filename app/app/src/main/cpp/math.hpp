#pragma once

#include <cinttypes>

constexpr auto round_up_to_pointer_size(uint32_t value) -> uint32_t {
    constexpr size_t POINTER_SIZE = sizeof(void*);

    return (value + POINTER_SIZE - 1 - ((value + POINTER_SIZE - 1) & (POINTER_SIZE - 1)));
}
