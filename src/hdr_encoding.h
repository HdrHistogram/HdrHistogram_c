//
// Created by barkerm on 9/09/15.
//

#ifndef HDR_ENCODING_H
#define HDR_ENCODING_H

#include <stdint.h>

int zig_zag_encode_i64(uint8_t* buffer, int64_t signed_value);
int zig_zag_decode_i64(const uint8_t* buffer, int64_t* signed_value);


#endif //HDR_HISTOGRAM_HDR_ENCODING_H
