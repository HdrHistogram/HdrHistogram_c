/* Released to the public domain under CC0. Codec stubs for no-zlib builds. */
#include <hdr/hdr_packed_histogram.h>

int hdr_packed_encode_compressed(
    const struct hdr_packed_histogram* h, uint8_t** bytes, size_t* length)
{
    (void)h; (void)bytes; (void)length;
    return -1;
}

int hdr_packed_decode_compressed(
    uint8_t* bytes, size_t length, struct hdr_packed_histogram** result)
{
    (void)bytes; (void)length; (void)result;
    return -1;
}
