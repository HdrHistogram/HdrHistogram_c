//
// Created by barkerm on 9/09/15.
//

#include "hdr_encoding.h"

/**
 * This class provides encoding and decoding methods for writing and reading
 * ZigZag-encoded LEB128 (Little Endian Base 128) values to/from a {@link ByteBuffer}.
 * LEB128's variable length encoding provides for using a smaller nuber of bytes
 * for smaller values, and the use of ZigZag encoding allows small (closer to zero)
 * negative values to use fewer bytes. Details on both LEB128 and ZigZag can be
 * readily found elsewhere.
 */

/**
 * Writes a int64_t value to the given buffer in LEB128 ZigZag encoded format
 *
 * @param buffer the buffer to write to
 * @param signed_value  the value to write to the buffer
 * @return the number of bytes written to the buffer
 */
int zig_zag_encode_i64(uint8_t* buffer, int64_t signed_value)
{
    uint64_t value = (uint64_t) signed_value;

    value = (value << 1) ^ (value >> 63);
    int bytesWritten = 0;
    if (value >> 7 == 0)
    {
        buffer[0] = (uint8_t) value;
        bytesWritten = 1;
    }
    else
    {
        buffer[0] = (uint8_t) ((value & 0x7F) | 0x80);
        if (value >> 14 == 0)
        {
            buffer[1] = (uint8_t) (value >> 7);
            bytesWritten = 2;
        }
        else
        {
            buffer[1] = (uint8_t) ((value >> 7 | 0x80));
            if (value >> 21 == 0)
            {
                buffer[2] = (uint8_t) (value >> 14);
                bytesWritten = 3;
            }
            else
            {
                buffer[2] = (uint8_t) (value >> 14 | 0x80);
                if (value >> 28 == 0)
                {
                    buffer[3] = (uint8_t) (value >> 21);
                    bytesWritten = 4;
                }
                else
                {
                    buffer[3] = (uint8_t) (value >> 21 | 0x80);
                    if (value >> 35 == 0)
                    {
                        buffer[4] = (uint8_t) (value >> 28);
                        bytesWritten = 5;
                    }
                    else
                    {
                        buffer[4] = (uint8_t) (value >> 28 | 0x80);
                        if (value >> 42 == 0)
                        {
                            buffer[5] = (uint8_t) (value >> 35);
                            bytesWritten = 6;
                        }
                        else
                        {
                            buffer[5] = (uint8_t) (value >> 35 | 0x80);
                            if (value >> 49 == 0)
                            {
                                buffer[6] = (uint8_t) (value >> 42);
                                bytesWritten = 7;
                            }
                            else
                            {
                                buffer[6] = (uint8_t) (value >> 42 | 0x80);
                                if (value >> 56 == 0)
                                {
                                    buffer[7] = (uint8_t) (value >> 49);
                                    bytesWritten = 8;
                                }
                                else
                                {
                                    buffer[7] = (uint8_t) (value >> 49 | 0x80);
                                    buffer[8] = (uint8_t) (value >> 56);
                                    bytesWritten = 9;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return bytesWritten;
}

/**
 * Read an LEB128 ZigZag encoded long value from the given buffer
 *
 * @param buffer the buffer to read from
 * @param retVal out value to capture the read value
 * @return the number of bytes read from the buffer
 */
int zig_zag_decode_i64(const uint8_t* buffer, int64_t* retVal)
{
    uint64_t v = buffer[0];
    uint64_t value = v & 0x7F;
    int bytesRead = 1;
    if ((v & 0x80) != 0)
    {
        bytesRead = 2;
        v = buffer[1];
        value |= (v & 0x7F) << 7;
        if ((v & 0x80) != 0)
        {
            bytesRead = 3;
            v = buffer[2];
            value |= (v & 0x7F) << 14;
            if ((v & 0x80) != 0)
            {
                bytesRead = 4;
                v = buffer[3];
                value |= (v & 0x7F) << 21;
                if ((v & 0x80) != 0)
                {
                    bytesRead = 5;
                    v = buffer[4];
                    value |= (v & 0x7F) << 28;
                    if ((v & 0x80) != 0)
                    {
                        bytesRead = 6;
                        v = buffer[5];
                        value |= (v & 0x7F) << 35;
                        if ((v & 0x80) != 0)
                        {
                            bytesRead = 7;
                            v = buffer[6];
                            value |= (v & 0x7F) << 42;
                            if ((v & 0x80) != 0)
                            {
                                bytesRead = 8;
                                v = buffer[7];
                                value |= (v & 0x7F) << 49;
                                if ((v & 0x80) != 0)
                                {
                                    bytesRead = 9;
                                    v = buffer[8];
                                    value |= v << 56;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    value = (value >> 1) ^ (-(value & 1));
    *retVal = (int64_t) value;

    return bytesRead;
}