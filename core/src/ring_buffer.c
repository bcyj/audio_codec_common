#include <audio_common/ring_buffer.h>
#include <stdint.h>
#include <string.h>

/* Return the smaller of two element counts. */
static size_t smaller(size_t a, size_t b) {
    return a < b ? a : b;
}

/* Bind caller-owned storage and initialize an empty allocation-free ring buffer. */
int audio_ring_init(AudioRingBuffer *ring, void *storage, size_t capacity, size_t element_size) {
    if (!ring || !element_size || (capacity && !storage) || capacity > SIZE_MAX / element_size)
        return 0;
    ring->storage = storage;
    ring->element_size = element_size;
    ring->capacity = capacity;
    ring->read_index = 0;
    ring->write_index = 0;
    ring->used = 0;
    return 1;
}

/* Clear stored elements and reset positions while retaining the storage configuration. */
void audio_ring_reset(AudioRingBuffer *ring) {
    ring->read_index = 0;
    ring->write_index = 0;
    ring->used = 0;
}

/* Return the number of readable elements. */
size_t audio_ring_size(const AudioRingBuffer *ring) {
    return ring->used;
}

/* Return the number of writable elements. */
size_t audio_ring_free(const AudioRingBuffer *ring) {
    return ring->capacity - ring->used;
}

/* Return up to two contiguous readable spans without consuming elements. */
size_t audio_ring_read_spans(const AudioRingBuffer *ring, size_t maximum,
                             AudioRingConstSpan spans[2]) {
    size_t count = smaller(maximum, ring->used);
    size_t first = smaller(count, ring->capacity - ring->read_index);
    spans[0].data = first ? ring->storage + ring->read_index * ring->element_size : NULL;
    spans[0].length = first;
    spans[1].data = count > first ? ring->storage : NULL;
    spans[1].length = count - first;
    return count;
}

/* Return up to two contiguous writable spans without committing elements. */
size_t audio_ring_write_spans(AudioRingBuffer *ring, size_t maximum, AudioRingSpan spans[2]) {
    size_t count = smaller(maximum, audio_ring_free(ring));
    size_t first = smaller(count, ring->capacity - ring->write_index);
    spans[0].data = first ? ring->storage + ring->write_index * ring->element_size : NULL;
    spans[0].length = first;
    spans[1].data = count > first ? ring->storage : NULL;
    spans[1].length = count - first;
    return count;
}

/* Consume elements and advance the read position. */
int audio_ring_consume(AudioRingBuffer *ring, size_t count) {
    if (count > ring->used)
        return 0;
    if (ring->capacity) {
        size_t tail = ring->capacity - ring->read_index;
        ring->read_index = count >= tail ? count - tail : ring->read_index + count;
    }
    ring->used -= count;
    return 1;
}

/* Commit written elements and advance the write position. */
int audio_ring_commit(AudioRingBuffer *ring, size_t count) {
    if (count > audio_ring_free(ring))
        return 0;
    if (ring->capacity) {
        size_t tail = ring->capacity - ring->write_index;
        ring->write_index = count >= tail ? count - tail : ring->write_index + count;
    }
    ring->used += count;
    return 1;
}

/* Copy and commit as many input elements as fit, including a wrapped second span. */
size_t audio_ring_write(AudioRingBuffer *ring, const void *input, size_t count) {
    AudioRingSpan spans[2];
    size_t transferred;
    if (!input && count)
        return 0;
    transferred = audio_ring_write_spans(ring, count, spans);
    if (spans[0].length)
        memcpy(spans[0].data, input, spans[0].length * ring->element_size);
    if (spans[1].length)
        memcpy(spans[1].data, (const uint8_t *)input + spans[0].length * ring->element_size,
               spans[1].length * ring->element_size);
    audio_ring_commit(ring, transferred);
    return transferred;
}

/* Copy and consume as many available elements as requested, including wraparound. */
size_t audio_ring_read(AudioRingBuffer *ring, void *output, size_t count) {
    AudioRingConstSpan spans[2];
    size_t transferred;
    if (!output && count)
        return 0;
    transferred = audio_ring_read_spans(ring, count, spans);
    if (spans[0].length)
        memcpy(output, spans[0].data, spans[0].length * ring->element_size);
    if (spans[1].length)
        memcpy((uint8_t *)output + spans[0].length * ring->element_size, spans[1].data,
               spans[1].length * ring->element_size);
    audio_ring_consume(ring, transferred);
    return transferred;
}
