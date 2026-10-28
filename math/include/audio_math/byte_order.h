/* Unaligned integer access for V4/V5 extension protocols and byte streams. */
#ifndef AUDIO_MATH_BYTE_ORDER_H
#define AUDIO_MATH_BYTE_ORDER_H
#include <audio_math/compiler.h>
#include <stdint.h>
#include <string.h>

/* Read a little-endian 16-bit integer from any byte address. */
static inline uint16_t audio_math_read_le16(const void *ptr) {
    const uint8_t *bytes = (const uint8_t *)ptr;
    return (uint16_t)((uint16_t)bytes[0] | (uint16_t)bytes[1] << 8);
}
/* Read a little-endian 24-bit integer as zero-extended bits. */
static inline uint32_t audio_math_read_le24(const void *ptr) {
    const uint8_t *bytes = (const uint8_t *)ptr;
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16;
}
/* Read a little-endian 32-bit integer from any byte address. */
static inline uint32_t audio_math_read_le32(const void *ptr) {
    const uint8_t *bytes = (const uint8_t *)ptr;
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}
/* Read a little-endian 64-bit integer without alignment requirements. */
static inline uint64_t audio_math_read_le64(const void *ptr) {
    const uint8_t *bytes = (const uint8_t *)ptr;
    return (uint64_t)audio_math_read_le32(bytes) | (uint64_t)audio_math_read_le32(bytes + 4) << 32;
}
/* Write little-endian 16-bit data; fixed-size copying permits safe compiler-generated unaligned stores. */
static AUDIO_MATH_FORCE_INLINE void audio_math_write_le16(void *ptr, uint16_t value) {
#if defined(_WIN32) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    memcpy(ptr, &value, sizeof value);
#else
    uint8_t *bytes = (uint8_t *)ptr;
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
#endif
}
/* Write little-endian 32-bit data; unknown/big-endian hosts encode bytewise without alignment assumptions. */
static AUDIO_MATH_FORCE_INLINE void audio_math_write_le32(void *ptr, uint32_t value) {
#if defined(_WIN32) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    memcpy(ptr, &value, sizeof value);
#else
    uint8_t *bytes = (uint8_t *)ptr;
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
#endif
}
/* Write an eight-byte integer in little-endian order. */
static inline void audio_math_write_le64(void *ptr, uint64_t value) {
    uint8_t *bytes = (uint8_t *)ptr;
    for (unsigned i = 0; i < 8; ++i)
        bytes[i] = (uint8_t)(value >> (8 * i));
}
/* Write a little-endian integer using the specified one-to-four-byte width. */
static inline void audio_math_write_le_bytes(void *ptr, uint32_t value, unsigned bytes) {
    uint8_t *output = (uint8_t *)ptr;
    for (unsigned i = 0; i < bytes; ++i)
        output[i] = (uint8_t)(value >> (8 * i));
}

/* Read a big-endian 16-bit integer from an arbitrary byte address. */
static inline uint16_t audio_math_read_be16(const void *ptr) {
    const uint8_t *bytes = (const uint8_t *)ptr;
    return (uint16_t)((uint16_t)bytes[0] << 8 | (uint16_t)bytes[1]);
}

/* Read a big-endian 24-bit integer from an arbitrary byte address and zero-extend it. */
static inline uint32_t audio_math_read_be24(const void *ptr) {
    const uint8_t *bytes = (const uint8_t *)ptr;
    return (uint32_t)bytes[0] << 16 | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2];
}

/* Read a big-endian 32-bit integer from any byte address. */
static inline uint32_t audio_math_read_be32(const void *ptr) {
    const uint8_t *bytes = (const uint8_t *)ptr;
    return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 | (uint32_t)bytes[2] << 8 | bytes[3];
}

/* Write a 32-bit integer to a byte buffer in big-endian order. */
static inline void audio_math_write_be32(void *ptr, uint32_t value) {
    uint8_t *bytes = (uint8_t *)ptr;
    bytes[0] = (uint8_t)(value >> 24);
    bytes[1] = (uint8_t)(value >> 16);
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)value;
}

/* Read a potentially unaligned native-endian protocol payload field. */
static inline uint32_t audio_math_read_native32(const void *ptr) {
    uint32_t value;
    memcpy(&value, ptr, sizeof(value));
    return value;
}
#endif
