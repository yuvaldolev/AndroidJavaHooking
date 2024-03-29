#pragma once

#include <cinttypes>
#include <memory>

class TrampolineGenerator {
public:
    static constexpr size_t TRAMPOLINE_MEMORY_SIZE = 4096;

    TrampolineGenerator(uint8_t entrypoint_offset);

    auto generate(void *method, void *entrypoint = nullptr) -> void*;

private:
    static auto allocate_trampoline_memory() -> void*;

    uint8_t *m_trampoline_memory_cursor = nullptr;
    uint8_t *m_trampoline_memory_end = nullptr;
    size_t m_trampoline_size;
    std::unique_ptr<uint8_t[]> m_trampoline;
    size_t m_backup_trampoline_size;
    std::unique_ptr<uint8_t[]> m_backup_trampoline;
};
