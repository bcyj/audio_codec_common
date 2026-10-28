#ifndef AUDIO_COMMON_RING_BUFFER_H
#define AUDIO_COMMON_RING_BUFFER_H

#include <stddef.h>
#include <stdint.h>

typedef struct AudioRingBuffer {
    uint8_t *storage;
    size_t element_size;
    size_t capacity;
    size_t read_index;
    size_t write_index;
    size_t used;
} AudioRingBuffer;

typedef struct AudioRingSpan {
    void *data;
    size_t length;
} AudioRingSpan;

typedef struct AudioRingConstSpan {
    const void *data;
    size_t length;
} AudioRingConstSpan;

#ifdef __cplusplus
extern "C" {
#endif

/* Counts and span lengths are elements, not bytes. Storage remains owned by
 * the caller and no operation allocates memory. */
int audio_ring_init(AudioRingBuffer *ring, void *storage, size_t capacity, size_t element_size);
void audio_ring_reset(AudioRingBuffer *ring);
size_t audio_ring_size(const AudioRingBuffer *ring);
size_t audio_ring_free(const AudioRingBuffer *ring);

/* Peek/reserve return up to two physical spans without changing state.
 * consume/commit must be called separately after the caller has completed the
 * corresponding operation. */
size_t audio_ring_read_spans(const AudioRingBuffer *ring, size_t maximum,
                             AudioRingConstSpan spans[2]);
size_t audio_ring_write_spans(AudioRingBuffer *ring, size_t maximum, AudioRingSpan spans[2]);
int audio_ring_consume(AudioRingBuffer *ring, size_t count);
int audio_ring_commit(AudioRingBuffer *ring, size_t count);

/* Copying conveniences. They transfer as many elements as currently fit or
 * are available and return the transferred element count. */
size_t audio_ring_write(AudioRingBuffer *ring, const void *input, size_t count);
size_t audio_ring_read(AudioRingBuffer *ring, void *output, size_t count);

#ifdef __cplusplus
}
#endif
#endif
