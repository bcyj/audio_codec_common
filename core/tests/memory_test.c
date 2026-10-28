#include <audio_common/memory.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                            \
    do {                                                                                            \
        if (!(condition)) {                                                                         \
            fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #condition);         \
            return 1;                                                                               \
        }                                                                                           \
    } while (0)

typedef struct TestHost {
    size_t allocations;
    size_t releases;
} TestHost;

static void *host_allocate(void *context, size_t bytes) {
    TestHost *host = context;
    host->allocations++;
    return malloc(bytes);
}

static void host_release(void *context, void *memory) {
    TestHost *host = context;
    host->releases++;
    free(memory);
}

int main(void) {
    size_t i;
    TestHost host = {0};
    AudioAllocator allocator = {&host, host_allocate, host_release};
    uint8_t *object = audio_memory_calloc(&allocator, 7, 19);
    CHECK(object && !((uintptr_t)object & (AUDIO_MEMORY_ALIGNMENT - 1)));
    CHECK(host.allocations == 1);
    for (i = 0; i < 133; ++i)
        CHECK(object[i] == 0);
    audio_memory_free(object);
    CHECK(host.releases == 1);
    CHECK(!audio_memory_alloc(&allocator, SIZE_MAX));
    CHECK(!audio_memory_calloc(&allocator, SIZE_MAX, 2));

    _Alignas(AUDIO_MEMORY_ALIGNMENT) uint8_t storage[96];
    AudioMemoryArena arena;
    CHECK(!audio_memory_arena_init(&arena, storage, sizeof(storage)));
    CHECK(audio_memory_arena_take(&arena, 17, 16) == storage);
    uintptr_t expected = ((uintptr_t)(storage + 17) + 31u) & ~(uintptr_t)31u;
    CHECK(audio_memory_arena_take(&arena, 8, 32) == (void *)expected);
    size_t used = arena.used;
    CHECK(!audio_memory_arena_take(&arena, 80, 16) && arena.used == used);

    _Alignas(AUDIO_MEMORY_ALIGNMENT) uint8_t exact_storage[128];
    size_t exact_bytes = audio_memory_block_bytes(33);
    CHECK(exact_bytes && exact_bytes <= sizeof(exact_storage));
    CHECK(!audio_memory_block_bytes(0) && !audio_memory_block_bytes(SIZE_MAX));
    CHECK(!audio_memory_arena_init(&arena, exact_storage, exact_bytes));
    AudioAllocator arena_allocator = audio_memory_arena_allocator(&arena);
    void *exact = audio_memory_alloc(&arena_allocator, 33);
    CHECK(exact && !audio_memory_alloc(&arena_allocator, 1));
    audio_memory_free(exact);
    return 0;
}
