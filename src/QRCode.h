/**
 * The MIT License (MIT)
 *
 * This library is written and maintained by Richard Moore.
 * Major parts were derived from Project Nayuki's library.
 *
 * Copyright (c) 2017 Richard Moore     (https://github.com/ricmoo/QRCode)
 * Copyright (c) 2017 Project Nayuki    (https://www.nayuki.io/page/qr-code-generator-library)
 * Copyright (c) 2026 MomePP            (https://github.com/MomePP/QRCode)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 *  Special thanks to Nayuki (https://www.nayuki.io/) from which this library was
 *  heavily inspired and compared against.
 *
 *  See: https://github.com/nayuki/QR-Code-generator/tree/master/cpp
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace qrcode
{
    constexpr uint8_t MIN_VERSION = 1;
    constexpr uint8_t MAX_VERSION = 40;

    // ISO/IEC 18004 level names; LOW and HIGH are Arduino macros.
    enum class Ecc : uint8_t
    {
        L,
        M,
        Q,
        H
    };

    namespace detail
    {
        constexpr uint8_t SIZE_STEP               = 4;
        constexpr uint8_t SIZE_BASE               = 17;
        constexpr uint8_t BITS_PER_BYTE           = 8;
        constexpr uint8_t HIGH_BIT                = 0x80;
        constexpr uint8_t MAX_BLOCK_ECC_CODEWORDS = 30;

        // Modules left for data and ECC codewords once function patterns are drawn.
        inline constexpr uint16_t NUM_RAW_DATA_MODULES[MAX_VERSION] = {
            //  1,   2,   3,   4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,   15,   16,   17,
            208, 359, 567, 807, 1079, 1383, 1568, 1936, 2336, 2768, 3232, 3728, 4256, 4651, 5243, 5867, 6523,
            //   18,   19,   20,   21,    22,    23,    24,    25,   26,    27,     28,    29,    30,    31,
            7211, 7931, 8683, 9252, 10068, 10916, 11796, 12708, 13652, 14628, 15371, 16411, 17483, 18587,
            //    32,    33,    34,    35,    36,    37,    38,    39,    40
            19723, 20891, 22091, 23008, 24272, 25568, 26896, 28256, 29648};

        constexpr uint16_t bytesForBits(uint32_t bits)
        {
            return (bits + BITS_PER_BYTE - 1) / BITS_PER_BYTE;
        }
        constexpr uint16_t gridBytes(uint8_t size)
        {
            return bytesForBits(static_cast<uint32_t>(size) * size);
        }
        constexpr uint16_t codewordBytes(uint8_t version)
        {
            return bytesForBits(NUM_RAW_DATA_MODULES[version - 1]);
        }
        constexpr uint8_t bitMask(uint32_t offset)
        {
            return HIGH_BIT >> (offset % BITS_PER_BYTE);
        }

        // Caller-owned scratch, sized for the version passed to encode().
        struct Workspace
        {
            uint8_t *modules;        // gridBytes(size)
            uint8_t *isFunction;     // gridBytes(size)
            uint8_t *codewords;      // codewordBytes(version)
            uint8_t *interleaved;    // codewordBytes(version)
            uint8_t *rsCoefficients; // MAX_BLOCK_ECC_CODEWORDS
        };

        bool encode(uint8_t version, Ecc ecc, const uint8_t *data, size_t length, const Workspace &workspace);
    }

    constexpr uint8_t sizeOf(uint8_t version)
    {
        return detail::SIZE_STEP * version + detail::SIZE_BASE;
    }

    template <uint8_t Version>
    class QRCode
    {
        static_assert(Version >= MIN_VERSION && Version <= MAX_VERSION, "QR code version must be within 1..40");

    public:
        static constexpr uint8_t SIZE = sizeOf(Version);

        bool encode(const char *text, Ecc ecc) { return encodeData(reinterpret_cast<const uint8_t *>(text), strlen(text), ecc); }
        bool encode(const uint8_t *data, uint16_t length, Ecc ecc) { return encodeData(data, length, ecc); }

        bool module(uint8_t x, uint8_t y) const
        {
            if (x >= SIZE || y >= SIZE)
                return false;
            const uint16_t offset = y * SIZE + x;
            return (_modules[offset / detail::BITS_PER_BYTE] & detail::bitMask(offset)) != 0;
        }

    private:
        static constexpr uint16_t GRID_BYTES     = detail::gridBytes(SIZE);
        static constexpr uint16_t CODEWORD_BYTES = detail::codewordBytes(Version);

        bool encodeData(const uint8_t *data, size_t length, Ecc ecc)
        {
            uint8_t isFunction[GRID_BYTES];
            uint8_t codewords[CODEWORD_BYTES];
            uint8_t interleaved[CODEWORD_BYTES];
            uint8_t rsCoefficients[detail::MAX_BLOCK_ECC_CODEWORDS];
            return detail::encode(Version, ecc, data, length, {_modules, isFunction, codewords, interleaved, rsCoefficients});
        }

        uint8_t _modules[GRID_BYTES] = {};
    };
}
