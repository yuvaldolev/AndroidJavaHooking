#include <sys/mman.h>

#include <cerrno>
#include <cstring>
#include <system_error>

#include "trampoline_generator.hpp"
#include "math.hpp"

// Trampoline:
// 1. Set eax/rdi/r0/x0 to the hook ArtMethod addr
// 2. Jump into its entry point

// Trampoline for backup:
// 1. Set eax/rdi/r0/x0 to the target ArtMethod addr
// 2. Ret to the hardcoded original entry point

#if defined(__i386__)
// b8 78 56 34 12 ; mov eax, 0x12345678 (addr of the hook method)
// ff 70 20 ; push DWORD PTR [eax + 0x20]
// c3 ; ret
const uint8_t trampoline[] = {
    0x00, 0x00, 0x00, 0x00, // code_size_ in OatQuickMethodHeader
    0xb8, 0x78, 0x56, 0x34, 0x12,
    0xff, 0x70, 0x20,
    0xc3
};

// b8 78 56 34 12 ; mov eax, 0x12345678 (addr of the target method)
// 68 78 56 34 12 ; push 0x12345678 (original entry point of the target method)
// c3 ; ret
const uint8_t backup_trampoline[] = {
    0xb8, 0x78, 0x56, 0x34, 0x12,
    0x68, 0x78, 0x56, 0x34, 0x12,
    0xc3
};
#elif defined(__x86_64__)
// 48 bf 78 56 34 12 78 56 34 12 ; movabs rdi, 0x1234567812345678
// ff 77 20 ; push QWORD PTR [rdi + 0x20]
// c3 ; ret
const uint8_t trampoline[] = {
    0x00, 0x00, 0x00, 0x00, // code_size_ in OatQuickMethodHeader
    0x48, 0xbf, 0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12,
    0xff, 0x77, 0x20,
    0xc3
};

// 48 bf 78 56 34 12 78 56 34 12 ; movabs rdi, 0x1234567812345678
// 57 ; push rdi (original entry point of the target method)
// 48 bf 78 56 34 12 78 56 34 12 ; movabs rdi, 0x1234567812345678 (addr of the target method)
// c3 ; ret
const uint8_t backup_trampoline[] = {
    0x48, 0xbf, 0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12,
    0x57,
    0x48, 0xbf, 0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12,
    0xc3
};
#elif defined(__arm__)
// 00 00 9F E5 ; ldr r0, [pc, #0]
// 20 F0 90 E5 ; ldr pc, [r0, 0x20]
// 78 56 34 12 ; 0x12345678 (addr of the hook method)
const uint8_t trampoline[] = {
    0x00, 0x00, 0x00, 0x00, // code_size_ in OatQuickMethodHeader
    0x00, 0x00, 0x9f, 0xe5,
    0x20, 0xf0, 0x90, 0xe5,
    0x78, 0x56, 0x34, 0x12
};

// 0c 00 9F E5 ; ldr r0, [pc, #12]
// 01 00 2d e9 ; push {r0}
// 00 00 9F E5 ; ldr r0, [pc, #0]
// 00 80 bd e8 ; pop {pc}
// 78 56 34 12 ; 0x12345678 (addr of the hook method)
// 78 56 34 12 ; 0x12345678 (original entry point of the target method)
const uint8_t backup_trampoline[] = {
    0x0c, 0x00, 0x9f, 0xe5,
    0x01, 0x00, 0x2d, 0xe9,
    0x00, 0x00, 0x9f, 0xe5,
    0x00, 0x80, 0xbd, 0xe8,
    0x78, 0x56, 0x34, 0x12,
    0x78, 0x56, 0x34, 0x12
};
#elif defined(__aarch64__)
// 60 00 00 58 ; ldr x0, 12
// 10 00 40 F8 ; ldr x16, [x0, #0x00]
// 00 02 1f d6 ; br x16
// 78 56 34 12
// 89 67 45 23 ; 0x2345678912345678 (addr of the hook method)
const uint8_t trampoline[] = {
    0x00, 0x00, 0x00, 0x00, // code_size_ in OatQuickMethodHeader
    0x60, 0x00, 0x00, 0x58,
    0x10, 0x00, 0x40, 0xf8,
    0x00, 0x02, 0x1f, 0xd6,
    0x78, 0x56, 0x34, 0x12,
    0x89, 0x67, 0x45, 0x23
};

// 60 00 00 58 ; ldr x0, 12
// 90 00 00 58 ; ldr x16, 16
// 00 02 1f d6 ; br x16
// 78 56 34 12
// 89 67 45 23 ; 0x2345678912345678 (addr of the hook method)
// 78 56 34 12
// 89 67 45 23 ; 0x2345678912345678 (original entry point of the target method)
const uint8_t backup_trampoline[] = {
    0x60, 0x00, 0x00, 0x58,
    0x90, 0x00, 0x00, 0x58,
    0x00, 0x02, 0x1f, 0xd6,
    0x78, 0x56, 0x34, 0x12,
    0x89, 0x67, 0x45, 0x23,
    0x78, 0x56, 0x34, 0x12,
    0x89, 0x67, 0x45, 0x23
};
#else
# error Unsupported architecture
#endif

TrampolineGenerator::TrampolineGenerator(uint8_t entrypoint_offset) :
    m_trampoline_size{ sizeof(trampoline) },
    m_trampoline{ std::make_unique<uint8_t[]>(m_trampoline_size) },
    m_backup_trampoline_size{ sizeof(backup_trampoline) },
    m_backup_trampoline{ std::make_unique<uint8_t[]>(m_backup_trampoline_size) } {
    // Copy the trampoline to our allocated trampoline.
    memcpy(m_trampoline.get(), trampoline, m_trampoline_size);

    // Copy the backup trampoline to our allocated backup trampoline.
    memcpy(m_backup_trampoline.get(), backup_trampoline, m_backup_trampoline_size);

#if defined(__i386__)
    m_trampoline[11] = entrypoint_offset;
#elif defined(__x86_64__)
    m_trampoline[16] = entrypoint_offset;
#elif defined(__arm__)
    m_trampoline[8] = entrypoint_offset;
#elif defined(__aarch64__)
    m_trampoline[9] |= entrypoint_offset << 4;
    m_trampoline[10] |= entrypoint_offset >> 4;
#else
# error Unsupported architecture
#endif
}

auto TrampolineGenerator::generate(void *method, void *entrypoint /* = nullptr */) -> void* {
    // Determine which trampoline should be retreived, the regular
    // trampoline or the backup trampoline.
    uint8_t *trampoline;
    size_t trampoline_size;
    if (nullptr == entrypoint) {
        // Retrieve the regular trampoline.
        trampoline = m_trampoline.get();
        trampoline_size = m_trampoline_size;
    } else {
        trampoline = m_backup_trampoline.get();
        trampoline_size = m_backup_trampoline_size;
    }

    // Allocate new memory for the trampoline if non is available.
    if (m_trampoline_memory_cursor + trampoline_size > m_trampoline_memory_end) {
        m_trampoline_memory_cursor = reinterpret_cast<uint8_t*>(allocate_trampoline_memory());
        m_trampoline_memory_end = m_trampoline_memory_cursor + TRAMPOLINE_MEMORY_SIZE;
    }

    // Copy the trampoline to the allocated memory.
    auto *result_trampoline = m_trampoline_memory_cursor; 
    memcpy(result_trampoline, trampoline, trampoline_size);
    
    // Replace the stub address with the actual method addresses.
#if defined(__i386__)
    if(nullptr == entrypoint) {
        memcpy(result_trampoline + 5, &method, sizeof(void*));
    } else {
        memcpy(result_trampoline + 1, &method, sizeof(void*);
        memcpy(result_trampoline + 6, &entrypoint, sizeof(void*);
    }
#elif defined(__x86_64__)
    if(nullptr == entrypoint) {
        memcpy(result_trampoline + 6, &method, sizeof(void*));
    } else {
        memcpy(result_trampoline + 2, &entrypoint, sizeof(void*));
        memcpy(result_trampoline + 13, &method, sizeof(void*));
    }
#elif defined(__arm__)
    if(nullptr == entrypoint) {
        memcpy(result_trampoline + 12, &method, sizeof(void*));
    } else {
        memcpy(result_trampoline + 16, &method, sizeof(void*));
        memcpy(result_trampoline + 20, &entrypoint, sizeof(void*));
    }
#elif defined(__aarch64__)
    if(nullptr == entrypoint) {
        memcpy(result_trampoline + 16, &method, sizeof(void*));
    } else {
        memcpy(result_trampoline + 12, &method, sizeof(void*));
        memcpy(result_trampoline + 20, &entrypoint, sizeof(void*));
    }
#else
# error Unsupported architecture
#endif

    // Skip 4 bytes of `code_size_`.
    if (nullptr == entrypoint) {
        result_trampoline += 4;
    }

    // Keep each trampoline aligned in memory.
    m_trampoline_memory_cursor += round_up_to_pointer_size(trampoline_size);

    return result_trampoline;
}

auto TrampolineGenerator::allocate_trampoline_memory() -> void* {
    auto *trampoline_memory = mmap(
            nullptr,
            TRAMPOLINE_MEMORY_SIZE,
            PROT_READ | PROT_WRITE | PROT_EXEC,
            MAP_ANONYMOUS | MAP_PRIVATE,
            -1,
            0
    );
    if (MAP_FAILED == trampoline_memory) {
        throw std::system_error{ errno, std::generic_category(), "mmap failed" };
    }

    return trampoline_memory;
}
