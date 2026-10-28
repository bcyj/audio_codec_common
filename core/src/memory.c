#include <audio_common/memory.h>

#include <string.h>

typedef struct AudioMemoryBlock {
    AudioAllocator allocator;
    void *original;
} AudioMemoryBlock;

/* Locates the ownership record immediately preceding a payload. */
static AudioMemoryBlock *memory_block(const void *memory) {
    return (AudioMemoryBlock *)((uint8_t *)memory - sizeof(AudioMemoryBlock));
}

int audio_memory_align_size(size_t bytes, size_t alignment, size_t *result) {
    if (!result || !alignment || (alignment & (alignment - 1)) ||
        bytes > SIZE_MAX - (alignment - 1))
        return -1;
    *result = (bytes + alignment - 1) & ~(alignment - 1);
    return 0;
}

int audio_memory_arena_init(AudioMemoryArena *arena, void *memory, size_t bytes) {
    uintptr_t begin, end, arena_begin, arena_end;
    if (!arena || !memory || !bytes || (uintptr_t)memory % AUDIO_MEMORY_ALIGNMENT ||
        bytes > UINTPTR_MAX - (uintptr_t)memory ||
        sizeof(*arena) > UINTPTR_MAX - (uintptr_t)arena)
        return -1;
    begin = (uintptr_t)memory;
    end = begin + bytes;
    arena_begin = (uintptr_t)arena;
    arena_end = arena_begin + sizeof(*arena);
    if (arena_begin < end && begin < arena_end)
        return -1;
    *arena = (AudioMemoryArena){memory, bytes, 0};
    return 0;
}

void *audio_memory_arena_take(AudioMemoryArena *arena, size_t bytes, size_t alignment) {
    size_t address, aligned, offset;
    if (!arena || !arena->base || !bytes || arena->used > arena->capacity ||
        arena->used > UINTPTR_MAX - (uintptr_t)arena->base)
        return NULL;
    address = (uintptr_t)arena->base + arena->used;
    if (audio_memory_align_size(address, alignment, &aligned) ||
        aligned < (uintptr_t)arena->base)
        return NULL;
    offset = aligned - (uintptr_t)arena->base;
    if (offset > arena->capacity || bytes > arena->capacity - offset)
        return NULL;
    arena->used = offset + bytes;
    return arena->base + offset;
}

static void *arena_allocate(void *context, size_t bytes) {
    return audio_memory_arena_take(context, bytes, AUDIO_MEMORY_ALIGNMENT);
}

static void arena_release(void *context, void *memory) {
    (void)context;
    (void)memory;
}

AudioAllocator audio_memory_arena_allocator(AudioMemoryArena *arena) {
    return (AudioAllocator){arena, arena_allocate, arena_release};
}

static size_t allocation_bytes(size_t bytes) {
    const size_t overhead = sizeof(AudioMemoryBlock) + AUDIO_MEMORY_ALIGNMENT - 1;
    return bytes && bytes <= SIZE_MAX - overhead ? bytes + overhead : 0;
}

size_t audio_memory_block_bytes(size_t payload_bytes) {
    size_t bytes = allocation_bytes(payload_bytes), aligned;
    return bytes && !audio_memory_align_size(bytes, AUDIO_MEMORY_ALIGNMENT, &aligned) ? aligned : 0;
}

void *audio_memory_alloc(const AudioAllocator *allocator, size_t bytes) {
    size_t total = allocation_bytes(bytes);
    AudioAllocator owner;
    void *original;
    uintptr_t raw, aligned;
    AudioMemoryBlock *block;
    if (!total || !allocator || !allocator->allocate || !allocator->release)
        return NULL;
    owner = *allocator;
    original = owner.allocate(owner.context, total);
    if (!original)
        return NULL;
    raw = (uintptr_t)original;
    if (raw > UINTPTR_MAX - total) {
        owner.release(owner.context, original);
        return NULL;
    }
    aligned = (raw + sizeof(AudioMemoryBlock) + AUDIO_MEMORY_ALIGNMENT - 1) &
              ~(uintptr_t)(AUDIO_MEMORY_ALIGNMENT - 1);
    block = (void *)(aligned - sizeof(*block));
    *block = (AudioMemoryBlock){owner, original};
    return (void *)aligned;
}

void *audio_memory_calloc(const AudioAllocator *allocator, size_t count, size_t bytes) {
    size_t total;
    void *memory;
    if (!count || !bytes || count > SIZE_MAX / bytes)
        return NULL;
    total = count * bytes;
    memory = audio_memory_alloc(allocator, total);
    if (memory)
        memset(memory, 0, total);
    return memory;
}

void audio_memory_free(void *memory) {
    AudioMemoryBlock block;
    if (!memory)
        return;
    block = *memory_block(memory);
    block.allocator.release(block.allocator.context, block.original);
}

