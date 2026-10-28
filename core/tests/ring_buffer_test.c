#include <audio_common/ring_buffer.h>
#include <stdint.h>
#include <string.h>

#define CHECK(x)                                                                                  \
    do {                                                                                          \
        if (!(x))                                                                                 \
            return __LINE__;                                                                      \
    } while (0)

int main(void) {
    AudioRingBuffer ring;
    AudioRingConstSpan read_spans[2];
    AudioRingSpan write_spans[2];
    int32_t storage[5], input[] = {1, 2, 3, 4, 5, 6, 7}, output[5] = {0};

    CHECK(audio_ring_init(&ring, storage, 5, sizeof(storage[0])));
    CHECK(audio_ring_write(&ring, input, 4) == 4);
    CHECK(audio_ring_read(&ring, output, 3) == 3);
    CHECK(!memcmp(output, input, 3 * sizeof(input[0])));
    CHECK(audio_ring_write(&ring, input + 4, 3) == 3);
    CHECK(audio_ring_size(&ring) == 4 && audio_ring_free(&ring) == 1);
    CHECK(audio_ring_read_spans(&ring, 4, read_spans) == 4);
    CHECK(read_spans[0].length == 2 && read_spans[1].length == 2);
    CHECK(((const int32_t *)read_spans[0].data)[0] == 4);
    CHECK(((const int32_t *)read_spans[1].data)[0] == 6);
    CHECK(audio_ring_consume(&ring, 4) && !audio_ring_size(&ring));
    CHECK(audio_ring_write_spans(&ring, 5, write_spans) == 5);
    CHECK(write_spans[0].length + write_spans[1].length == 5);
    CHECK(audio_ring_commit(&ring, 5) && audio_ring_size(&ring) == 5);
    CHECK(!audio_ring_commit(&ring, 1) && !audio_ring_consume(&ring, 6));
    audio_ring_reset(&ring);
    CHECK(!audio_ring_size(&ring) && audio_ring_free(&ring) == 5);
    return 0;
}
