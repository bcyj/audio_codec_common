#ifndef AUDIO_COMMON_MEMORY_H
#define AUDIO_COMMON_MEMORY_H

#include <stddef.h>
#include <stdint.h>

#if defined(_MSC_VER)
#define AUDIO_MEMORY_CALL __cdecl
#elif defined(_WIN32) && defined(__i386__)
#define AUDIO_MEMORY_CALL __attribute__((cdecl))
#else
#define AUDIO_MEMORY_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_MEMORY_ALIGNMENT 16u

/* Host allocator interface. The context must outlive every object it creates. */
typedef struct AudioAllocator {
    void *context;
    void *(AUDIO_MEMORY_CALL *allocate)(void *context, size_t bytes);
    void(AUDIO_MEMORY_CALL *release)(void *context, void *memory);
} AudioAllocator;

/* Monotonic initialization arena; sub-blocks are released with the whole arena. */
typedef struct AudioMemoryArena {
    uint8_t *base;
    size_t capacity;
    size_t used;
} AudioMemoryArena;

/* Computes an aligned size with overflow checks; alignment must be a power of two. */
int AUDIO_MEMORY_CALL audio_memory_align_size(size_t bytes, size_t alignment, size_t *result);

/* Binds caller-owned contiguous memory; the arena descriptor must not overlap it. */
int AUDIO_MEMORY_CALL audio_memory_arena_init(AudioMemoryArena *arena, void *memory, size_t bytes);

/* Takes an aligned sub-block without advancing the cursor when capacity is insufficient. */
void *AUDIO_MEMORY_CALL audio_memory_arena_take(AudioMemoryArena *arena, size_t bytes,
                                                size_t alignment);

/* Adapts an arena to the allocator interface; release does not reclaim sub-blocks. */
AudioAllocator AUDIO_MEMORY_CALL audio_memory_arena_allocator(AudioMemoryArena *arena);

/* Returns workspace capacity for a payload, ownership metadata, and alignment padding. */
size_t AUDIO_MEMORY_CALL audio_memory_block_bytes(size_t payload_bytes);

/* Creates a 16-byte-aligned object and records its explicit host allocator. */
void *AUDIO_MEMORY_CALL audio_memory_alloc(const AudioAllocator *allocator, size_t bytes);

/* Allocates and clears an array with overflow checks. */
void *AUDIO_MEMORY_CALL audio_memory_calloc(const AudioAllocator *allocator, size_t count,
                                            size_t bytes);

/* Releases the original address through the host interface recorded before the object. */
void AUDIO_MEMORY_CALL audio_memory_free(void *memory);

#ifdef __cplusplus
}
#endif
#endif
