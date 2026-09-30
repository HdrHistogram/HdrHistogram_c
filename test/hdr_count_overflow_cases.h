/* Test-only V0/V1/V2 count-frame builder. Released to the public domain. */
#ifndef HDR_COUNT_OVERFLOW_CASES_H
#define HDR_COUNT_OVERFLOW_CASES_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* Internal codec entry points; the decoder is not in the public header. */
int zig_zag_encode_i64(uint8_t*, int64_t);
int hdr_decode_compressed(uint8_t*, size_t, struct hdr_histogram**);

static void put32(uint8_t* dst, uint32_t v)
{
    for (int j = 3; j >= 0; j--) { dst[j] = (uint8_t)v; v >>= 8U; }
}

static void put64(uint8_t* dst, uint64_t v)
{
    for (int j = 7; j >= 0; j--) { dst[j] = (uint8_t)v; v >>= 8U; }
}

/* version 0 uses the full fixed-width array; V1 limits it to four slots. */
static uint8_t* count_frame(int version, const int64_t counts[3], size_t* length)
{
    struct hdr_histogram* geometry = NULL;
    uint8_t *raw, *compressed;
    size_t raw_size, used;
    uLongf bound;
    if (hdr_init(1, 1000, 1, &geometry) != 0) return NULL;
    raw_size = version == 0 ? 32 + (size_t)geometry->counts_len * 8 : 40 + 40;
    raw = (uint8_t*)calloc(1, raw_size);
    if (raw == NULL) { hdr_close(geometry); return NULL; }
    if (version == 0)
    {
        put32(raw, UINT32_C(0x1c849388));
        put32(raw + 4, 1);
        put64(raw + 8, 1);
        put64(raw + 16, 1000);
        for (int j = 0; j < 3; j++) put64(raw + 32 + (size_t)(j + 1) * 8, (uint64_t)counts[j]);
        used = raw_size;
    }
    else
    {
        put32(raw, version == 1 ? UINT32_C(0x1c849381) : UINT32_C(0x1c849313));
        put32(raw + 12, 1);
        put64(raw + 16, 1);
        put64(raw + 24, 1000);
        put64(raw + 32, UINT64_C(0x3ff0000000000000));
        if (version == 1)
        {
            put32(raw + 4, 32);
            for (int j = 0; j < 3; j++) put64(raw + 40 + (size_t)(j + 1) * 8, (uint64_t)counts[j]);
            used = 72;
        }
        else
        {
            used = 40;
            used += (size_t)zig_zag_encode_i64(raw + used, 0);
            for (int j = 0; j < 3; j++) used += (size_t)zig_zag_encode_i64(raw + used, counts[j]);
            put32(raw + 4, (uint32_t)(used - 40));
        }
    }
    hdr_close(geometry);
    bound = compressBound((uLong)used);
    compressed = (uint8_t*)malloc((size_t)bound + 8);
    if (compressed == NULL) { free(raw); return NULL; }
    if (compress(compressed + 8, &bound, raw, (uLong)used) != Z_OK)
    {
        free(raw);
        free(compressed);
        return NULL;
    }
    free(raw);
    put32(compressed, version == 0 ? UINT32_C(0x1c849389) :
        (version == 1 ? UINT32_C(0x1c849382) : UINT32_C(0x1c849314)));
    put32(compressed + 4, (uint32_t)bound);
    *length = (size_t)bound + 8;
    return compressed;
}
#endif
