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

#include "QRCode.h"

namespace qrcode::detail
{
    namespace
    {
        constexpr uint8_t NUM_ECC_LEVELS = 4;

        // Rows indexed by Ecc (L, M, Q, H), columns by version - 1.
        constexpr uint16_t NUM_ERROR_CORRECTION_CODEWORDS[NUM_ECC_LEVELS][MAX_VERSION] = {
            // 1,  2,  3,  4,  5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,   25,   26,   27,   28,   29,   30,   31,   32,   33,   34,   35,   36,   37,   38,   39,   40
            {7, 10, 15, 20, 26, 36, 40, 48, 60, 72, 80, 96, 104, 120, 132, 144, 168, 180, 196, 224, 224, 252, 270, 300, 312, 336, 360, 390, 420, 450, 480, 510, 540, 570, 570, 600, 630, 660, 720, 750},
            {10, 16, 26, 36, 48, 64, 72, 88, 110, 130, 150, 176, 198, 216, 240, 280, 308, 338, 364, 416, 442, 476, 504, 560, 588, 644, 700, 728, 784, 812, 868, 924, 980, 1036, 1064, 1120, 1204, 1260, 1316, 1372},
            {13, 22, 36, 52, 72, 96, 108, 132, 160, 192, 224, 260, 288, 320, 360, 408, 448, 504, 546, 600, 644, 690, 750, 810, 870, 952, 1020, 1050, 1140, 1200, 1290, 1350, 1440, 1530, 1590, 1680, 1770, 1860, 1950, 2040},
            {17, 28, 44, 64, 88, 112, 130, 156, 192, 224, 264, 308, 352, 384, 432, 480, 532, 588, 650, 700, 750, 816, 900, 960, 1050, 1110, 1200, 1260, 1350, 1440, 1530, 1620, 1710, 1800, 1890, 1980, 2100, 2220, 2310, 2430},
        };

        constexpr uint8_t NUM_ERROR_CORRECTION_BLOCKS[NUM_ECC_LEVELS][MAX_VERSION] = {
            // 1, 2, 3, 4, 5, 6, 7, 8, 9,10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40
            {1, 1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 4, 6, 6, 6, 6, 7, 8, 8, 9, 9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},
            {1, 1, 1, 2, 2, 4, 4, 4, 5, 5, 5, 8, 9, 9, 10, 10, 11, 13, 14, 16, 17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},
            {1, 1, 2, 2, 4, 4, 6, 6, 8, 8, 8, 10, 12, 16, 12, 17, 16, 18, 21, 20, 23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},
            {1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25, 25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},
        };

        // The format information encodes the levels out of order: L=01, M=00, Q=11, H=10.
        constexpr uint8_t ECC_FORMAT_BITS[NUM_ECC_LEVELS] = {0b01, 0b00, 0b11, 0b10};

        constexpr bool blockEccFitsCoefficientBuffer()
        {
            for (uint8_t level = 0; level < NUM_ECC_LEVELS; level++)
                for (uint8_t index = 0; index < MAX_VERSION; index++)
                    if (NUM_ERROR_CORRECTION_CODEWORDS[level][index] / NUM_ERROR_CORRECTION_BLOCKS[level][index] > MAX_BLOCK_ECC_CODEWORDS)
                        return false;
            return true;
        }
        static_assert(blockEccFitsCoefficientBuffer(), "MAX_BLOCK_ECC_CODEWORDS must bound every block's ECC length");

        enum class Mode : uint8_t
        {
            NUMERIC,
            ALPHANUMERIC,
            BYTE
        };

        constexpr uint8_t MODE_INDICATOR_BITS   = 4;
        constexpr uint8_t MAX_CHARS_PER_GROUP   = 3;
        constexpr uint8_t NUM_CHAR_COUNT_RANGES = 3;

        // Character-count field widths grow above these versions.
        constexpr uint8_t CHAR_COUNT_RANGE_LAST_VERSION[NUM_CHAR_COUNT_RANGES - 1] = {9, 26};

        struct ModeSpec
        {
            uint8_t indicator;
            uint16_t radix;
            uint8_t charsPerGroup;
            uint8_t bitsPerGroup;
            uint8_t remainderBits[MAX_CHARS_PER_GROUP]; // indexed by characters left over after the last full group
            uint8_t charCountBits[NUM_CHAR_COUNT_RANGES];
        };

        // Indexed by Mode.
        constexpr ModeSpec MODE_SPECS[] = {
            {0b0001, 10, 3, 10, {0, 4, 7}, {10, 12, 14}},
            {0b0010, 45, 2, 11, {0, 6, 0}, {9, 11, 13}},
            {0b0100, 256, 1, 8, {0, 0, 0}, {8, 16, 16}},
        };

        constexpr int8_t NOT_ALPHANUMERIC            = -1;
        constexpr uint8_t ALPHANUMERIC_LETTER_OFFSET = 10;
        constexpr uint8_t ALPHANUMERIC_SYMBOL_OFFSET = 36;
        constexpr char ALPHANUMERIC_SYMBOLS[]        = " $%*+-./:";
        constexpr uint8_t NUM_ALPHANUMERIC_SYMBOLS   = sizeof(ALPHANUMERIC_SYMBOLS) - 1;

        int8_t alphanumericValue(uint8_t c)
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'A' && c <= 'Z')
                return c - 'A' + ALPHANUMERIC_LETTER_OFFSET;
            for (uint8_t i = 0; i < NUM_ALPHANUMERIC_SYMBOLS; i++)
                if (c == ALPHANUMERIC_SYMBOLS[i])
                    return ALPHANUMERIC_SYMBOL_OFFSET + i;
            return NOT_ALPHANUMERIC;
        }

        Mode selectMode(const uint8_t *data, size_t length)
        {
            bool numeric = true;
            for (size_t i = 0; i < length; i++)
            {
                if (alphanumericValue(data[i]) == NOT_ALPHANUMERIC)
                    return Mode::BYTE;
                numeric = numeric && data[i] >= '0' && data[i] <= '9';
            }
            return numeric ? Mode::NUMERIC : Mode::ALPHANUMERIC;
        }

        const ModeSpec &specOf(Mode mode)
        {
            return MODE_SPECS[static_cast<uint8_t>(mode)];
        }

        uint8_t charCountBits(const ModeSpec &spec, uint8_t version)
        {
            uint8_t range = 0;
            while (range < NUM_CHAR_COUNT_RANGES - 1 && version > CHAR_COUNT_RANGE_LAST_VERSION[range])
                range++;
            return spec.charCountBits[range];
        }

        uint32_t segmentBits(const ModeSpec &spec, uint8_t countBits, uint16_t length)
        {
            return MODE_INDICATOR_BITS + countBits
                   + static_cast<uint32_t>(length / spec.charsPerGroup) * spec.bitsPerGroup
                   + spec.remainderBits[length % spec.charsPerGroup];
        }

        // Bit buffers and grids, packed most significant bit first

        struct BitBuffer
        {
            uint8_t *bytes;
            uint32_t bitLength;

            void append(uint32_t value, uint8_t bitCount)
            {
                for (uint8_t i = bitCount; i-- > 0; bitLength++)
                    if ((value >> i) & 1)
                        bytes[bitLength / BITS_PER_BYTE] |= bitMask(bitLength);
            }
        };

        struct Grid
        {
            uint8_t *bits;
            uint8_t size;

            uint16_t offset(uint8_t x, uint8_t y) const { return y * size + x; }

            bool get(uint8_t x, uint8_t y) const
            {
                const uint16_t at = offset(x, y);
                return (bits[at / BITS_PER_BYTE] & bitMask(at)) != 0;
            }

            void set(uint8_t x, uint8_t y, bool dark)
            {
                const uint16_t at = offset(x, y);
                if (dark)
                    bits[at / BITS_PER_BYTE] |= bitMask(at);
                else
                    bits[at / BITS_PER_BYTE] &= ~bitMask(at);
            }

            void flip(uint8_t x, uint8_t y)
            {
                const uint16_t at = offset(x, y);
                bits[at / BITS_PER_BYTE] ^= bitMask(at);
            }
        };

        struct Symbol
        {
            Grid modules;
            Grid isFunction;

            void setFunction(uint8_t x, uint8_t y, bool dark)
            {
                modules.set(x, y, dark);
                isFunction.set(x, y, true);
            }
        };

        void appendSegment(BitBuffer &buffer, Mode mode, uint8_t countBits, const uint8_t *data, uint16_t length)
        {
            const ModeSpec &spec = specOf(mode);
            buffer.append(spec.indicator, MODE_INDICATOR_BITS);
            buffer.append(length, countBits);

            uint16_t group      = 0;
            uint8_t groupLength = 0;
            for (uint16_t i = 0; i < length; i++)
            {
                const uint8_t value = mode == Mode::BYTE ? data[i] : alphanumericValue(data[i]);
                group               = group * spec.radix + value;
                if (++groupLength == spec.charsPerGroup)
                {
                    buffer.append(group, spec.bitsPerGroup);
                    group       = 0;
                    groupLength = 0;
                }
            }
            if (groupLength > 0)
                buffer.append(group, spec.remainderBits[groupLength]);
        }

        constexpr uint8_t TIMING_PATTERN_POSITION = 6;
        constexpr uint8_t FINDER_CENTER           = 3;
        constexpr int8_t FINDER_RADIUS            = 4; // includes the light separator
        constexpr uint8_t FINDER_LIGHT_RING       = 2;
        constexpr int8_t ALIGNMENT_RADIUS         = 2;
        constexpr uint8_t ALIGNMENT_LIGHT_RING    = 1;

        constexpr uint8_t ALIGNMENT_VERSION_INTERVAL  = 7;
        constexpr uint8_t MIN_ALIGNMENT_COUNT         = 2;
        constexpr uint8_t MAX_ALIGNMENT_COUNT         = MAX_VERSION / ALIGNMENT_VERSION_INTERVAL + MIN_ALIGNMENT_COUNT;
        constexpr uint8_t IRREGULAR_ALIGNMENT_VERSION = 32;
        constexpr uint8_t IRREGULAR_ALIGNMENT_STEP    = 26;

        constexpr uint8_t FORMAT_POSITION   = 8;
        constexpr uint8_t FORMAT_INFO_BITS  = 15;
        constexpr uint8_t FORMAT_ECC_BITS   = 10;
        constexpr uint16_t FORMAT_GENERATOR = 0x537;
        constexpr uint16_t FORMAT_XOR_MASK  = 0x5412;
        constexpr uint8_t MASK_PATTERN_BITS = 3;
        constexpr uint8_t NUM_MASK_PATTERNS = 8;

        constexpr uint8_t VERSION_INFO_MIN_VERSION  = 7;
        constexpr uint8_t VERSION_INFO_BITS         = 18;
        constexpr uint8_t VERSION_INFO_ECC_BITS     = 12;
        constexpr uint16_t VERSION_INFO_GENERATOR   = 0x1F25;
        constexpr uint8_t VERSION_INFO_BLOCK_HEIGHT = 3;
        constexpr uint8_t VERSION_INFO_EDGE_OFFSET  = 11;

        constexpr uint8_t ZIGZAG_COLUMN_WIDTH = 2;

        uint8_t chebyshevDistance(int8_t dx, int8_t dy)
        {
            const uint8_t ax = dx < 0 ? -dx : dx;
            const uint8_t ay = dy < 0 ? -dy : dy;
            return ax > ay ? ax : ay;
        }

        // Appends the BCH remainder of `value` under `generator` (format and version information).
        uint32_t bchEncode(uint32_t value, uint8_t eccBits, uint16_t generator)
        {
            uint32_t remainder = value;
            for (uint8_t i = 0; i < eccBits; i++)
                remainder = (remainder << 1) ^ ((remainder >> (eccBits - 1)) * generator);
            return value << eccBits | remainder;
        }

        // XORs the data modules in this QR Code with the given mask pattern. Due to XOR's mathematical
        // properties, calling applyMask(m) twice with the same value is equivalent to no change at all.
        // This means it is possible to apply a mask, undo it, and try another mask. Note that a final
        // well-formed QR Code symbol needs exactly one mask applied (not zero, not two, etc.).
        bool maskInverts(uint8_t mask, uint8_t x, uint8_t y)
        {
            switch (mask)
            {
            case 0:
                return (x + y) % 2 == 0;
            case 1:
                return y % 2 == 0;
            case 2:
                return x % 3 == 0;
            case 3:
                return (x + y) % 3 == 0;
            case 4:
                return (x / 3 + y / 2) % 2 == 0;
            case 5:
                return x * y % 2 + x * y % 3 == 0;
            case 6:
                return (x * y % 2 + x * y % 3) % 2 == 0;
            case 7:
                return ((x + y) % 2 + x * y % 3) % 2 == 0;
            }
            return false;
        }

        void applyMask(Symbol &symbol, uint8_t mask)
        {
            const uint8_t size = symbol.modules.size;
            for (uint8_t y = 0; y < size; y++)
                for (uint8_t x = 0; x < size; x++)
                    if (!symbol.isFunction.get(x, y) && maskInverts(mask, x, y))
                        symbol.modules.flip(x, y);
        }

        // Draws a 9*9 finder pattern including the border separator, with the center module at (x, y).
        void drawFinderPattern(Symbol &symbol, uint8_t x, uint8_t y)
        {
            const uint8_t size = symbol.modules.size;
            for (int8_t dy = -FINDER_RADIUS; dy <= FINDER_RADIUS; dy++)
            {
                for (int8_t dx = -FINDER_RADIUS; dx <= FINDER_RADIUS; dx++)
                {
                    const int16_t xx = x + dx;
                    const int16_t yy = y + dy;
                    if (0 <= xx && xx < size && 0 <= yy && yy < size)
                    {
                        const uint8_t distance = chebyshevDistance(dx, dy);
                        symbol.setFunction(xx, yy, distance != FINDER_LIGHT_RING && distance != FINDER_RADIUS);
                    }
                }
            }
        }

        // Draws a 5*5 alignment pattern, with the center module at (x, y).
        void drawAlignmentPattern(Symbol &symbol, uint8_t x, uint8_t y)
        {
            for (int8_t dy = -ALIGNMENT_RADIUS; dy <= ALIGNMENT_RADIUS; dy++)
                for (int8_t dx = -ALIGNMENT_RADIUS; dx <= ALIGNMENT_RADIUS; dx++)
                    symbol.setFunction(x + dx, y + dy, chebyshevDistance(dx, dy) != ALIGNMENT_LIGHT_RING);
        }

        void drawAlignmentPatterns(Symbol &symbol, uint8_t version)
        {
            if (version == MIN_VERSION)
                return;

            const uint8_t count = version / ALIGNMENT_VERSION_INTERVAL + MIN_ALIGNMENT_COUNT;
            // ceil((size - 13) / (2 * count - 2)) * 2; the spec spaces version 32 irregularly.
            const uint8_t step = version == IRREGULAR_ALIGNMENT_VERSION
                                     ? IRREGULAR_ALIGNMENT_STEP
                                     : (version * SIZE_STEP + count * 2 + 1) / (count * 2 - 2) * 2;

            uint8_t positions[MAX_ALIGNMENT_COUNT];
            positions[0]     = TIMING_PATTERN_POSITION;
            uint8_t position = symbol.modules.size - 1 - TIMING_PATTERN_POSITION;
            for (uint8_t i = count - 1; i > 0; i--, position -= step)
                positions[i] = position;

            const uint8_t last = count - 1;
            for (uint8_t i = 0; i < count; i++)
            {
                for (uint8_t j = 0; j < count; j++)
                {
                    const bool finderCorner = (i == 0 && j == 0) || (i == 0 && j == last) || (i == last && j == 0);
                    if (!finderCorner)
                        drawAlignmentPattern(symbol, positions[i], positions[j]);
                }
            }
        }

        // Draws two copies of the format bits (with its own error correction code)
        // based on the given mask and error correction level.
        void drawFormatBits(Symbol &symbol, uint8_t eccFormatBits, uint8_t mask)
        {
            const uint8_t size  = symbol.modules.size;
            const uint32_t data = bchEncode(eccFormatBits << MASK_PATTERN_BITS | mask, FORMAT_ECC_BITS, FORMAT_GENERATOR) ^ FORMAT_XOR_MASK;
            const auto bit      = [data](uint8_t i)
            { return ((data >> i) & 1) != 0; };

            // First copy: down column 8 and left along row 8, skipping the timing patterns
            for (uint8_t i = 0; i < TIMING_PATTERN_POSITION; i++)
                symbol.setFunction(FORMAT_POSITION, i, bit(i));
            symbol.setFunction(FORMAT_POSITION, TIMING_PATTERN_POSITION + 1, bit(TIMING_PATTERN_POSITION));
            symbol.setFunction(FORMAT_POSITION, FORMAT_POSITION, bit(TIMING_PATTERN_POSITION + 1));
            symbol.setFunction(FORMAT_POSITION - 1, FORMAT_POSITION, bit(FORMAT_POSITION));
            for (uint8_t i = FORMAT_POSITION + 1; i < FORMAT_INFO_BITS; i++)
                symbol.setFunction(FORMAT_INFO_BITS - 1 - i, FORMAT_POSITION, bit(i));

            // Second copy: split between the top-right and bottom-left finders
            for (uint8_t i = 0; i < FORMAT_POSITION; i++)
                symbol.setFunction(size - 1 - i, FORMAT_POSITION, bit(i));
            for (uint8_t i = FORMAT_POSITION; i < FORMAT_INFO_BITS; i++)
                symbol.setFunction(FORMAT_POSITION, size - FORMAT_INFO_BITS + i, bit(i));

            // The always-dark module
            symbol.setFunction(FORMAT_POSITION, size - FORMAT_POSITION, true);
        }

        // Draws two copies of the version bits (with its own error correction code),
        // which only exist for 7 <= version <= 40.
        void drawVersion(Symbol &symbol, uint8_t version)
        {
            if (version < VERSION_INFO_MIN_VERSION)
                return;

            const uint8_t size  = symbol.modules.size;
            const uint32_t data = bchEncode(version, VERSION_INFO_ECC_BITS, VERSION_INFO_GENERATOR);
            for (uint8_t i = 0; i < VERSION_INFO_BITS; i++)
            {
                const bool dark = ((data >> i) & 1) != 0;
                const uint8_t a = size - VERSION_INFO_EDGE_OFFSET + i % VERSION_INFO_BLOCK_HEIGHT;
                const uint8_t b = i / VERSION_INFO_BLOCK_HEIGHT;
                symbol.setFunction(a, b, dark);
                symbol.setFunction(b, a, dark);
            }
        }

        void drawFunctionPatterns(Symbol &symbol, uint8_t version, uint8_t eccFormatBits)
        {
            const uint8_t size = symbol.modules.size;

            for (uint8_t i = 0; i < size; i++)
            {
                symbol.setFunction(TIMING_PATTERN_POSITION, i, i % 2 == 0);
                symbol.setFunction(i, TIMING_PATTERN_POSITION, i % 2 == 0);
            }

            // All corners except bottom right; overwrites some timing modules
            drawFinderPattern(symbol, FINDER_CENTER, FINDER_CENTER);
            drawFinderPattern(symbol, size - 1 - FINDER_CENTER, FINDER_CENTER);
            drawFinderPattern(symbol, FINDER_CENTER, size - 1 - FINDER_CENTER);

            drawAlignmentPatterns(symbol, version);

            // Dummy mask; marks the format area as function modules until the real mask is chosen
            drawFormatBits(symbol, eccFormatBits, 0);
            drawVersion(symbol, version);
        }

        // Draws the given sequence of 8-bit codewords (data and error correction) onto the entire
        // data area of this QR Code symbol. Function modules need to be marked off before this is called.
        void drawCodewords(Symbol &symbol, const uint8_t *codewords, uint32_t bitLength)
        {
            const uint8_t size = symbol.modules.size;
            uint32_t i         = 0;

            for (int16_t right = size - 1; right >= 1; right -= ZIGZAG_COLUMN_WIDTH)
            {
                if (right == TIMING_PATTERN_POSITION)
                    right--;
                const bool upwards = ((right + 1) / ZIGZAG_COLUMN_WIDTH) % 2 == 0;

                for (uint8_t vert = 0; vert < size; vert++)
                {
                    for (uint8_t j = 0; j < ZIGZAG_COLUMN_WIDTH; j++)
                    {
                        const uint8_t x = right - j;
                        const uint8_t y = upwards ? size - 1 - vert : vert;
                        // Remainder bits past bitLength stay light from the cleared grid.
                        if (!symbol.isFunction.get(x, y) && i < bitLength)
                        {
                            symbol.modules.set(x, y, (codewords[i / BITS_PER_BYTE] & bitMask(i)) != 0);
                            i++;
                        }
                    }
                }
            }
        }

        constexpr uint8_t PENALTY_N1 = 3;
        constexpr uint8_t PENALTY_N2 = 3;
        constexpr uint8_t PENALTY_N3 = 40;
        constexpr uint8_t PENALTY_N4 = 10;

        constexpr uint8_t PENALTY_RUN_LENGTH = 5;

        constexpr uint8_t FINDER_LIKE_BITS              = 11;
        constexpr uint16_t FINDER_LIKE_WINDOW           = (1 << FINDER_LIKE_BITS) - 1;
        constexpr uint16_t FINDER_LIKE_PATTERN          = 0x05D;
        constexpr uint16_t FINDER_LIKE_PATTERN_REVERSED = 0x5D0;

        // Dark share must lie within (45 - 5k)%..(55 + 5k)%, scaled by 20 to stay integral.
        constexpr uint8_t BALANCE_SCALE = 20;
        constexpr uint8_t BALANCE_LOW   = 9;
        constexpr uint8_t BALANCE_HIGH  = 11;

        bool isFinderLike(uint16_t window)
        {
            return window == FINDER_LIKE_PATTERN || window == FINDER_LIKE_PATTERN_REVERSED;
        }

        // Adjacent modules in a row (or column) having the same color.
        uint32_t runPenalty(const Grid &grid, bool columns)
        {
            uint32_t result = 0;
            for (uint8_t line = 0; line < grid.size; line++)
            {
                bool runColor = columns ? grid.get(line, 0) : grid.get(0, line);
                for (uint8_t position = 1, run = 1; position < grid.size; position++)
                {
                    const bool color = columns ? grid.get(line, position) : grid.get(position, line);
                    if (color != runColor)
                    {
                        runColor = color;
                        run      = 1;
                    }
                    else if (++run == PENALTY_RUN_LENGTH)
                        result += PENALTY_N1;
                    else if (run > PENALTY_RUN_LENGTH)
                        result++;
                }
            }
            return result;
        }

        // Calculates and returns the penalty score based on state of this QR Code's current modules.
        // This is used by the automatic mask choice algorithm to find the mask pattern that yields the lowest score.
        uint32_t penaltyScore(const Grid &modules)
        {
            const uint8_t size = modules.size;
            uint32_t result    = runPenalty(modules, false) + runPenalty(modules, true);
            uint32_t dark      = 0;

            for (uint8_t y = 0; y < size; y++)
            {
                uint16_t rowWindow = 0, columnWindow = 0;
                for (uint8_t x = 0; x < size; x++)
                {
                    const bool color = modules.get(x, y);

                    // 2*2 blocks of modules having same color
                    if (x > 0 && y > 0 && color == modules.get(x - 1, y - 1) && color == modules.get(x, y - 1) && color == modules.get(x - 1, y))
                        result += PENALTY_N2;

                    // Finder-like pattern in rows and columns
                    rowWindow    = ((rowWindow << 1) & FINDER_LIKE_WINDOW) | color;
                    columnWindow = ((columnWindow << 1) & FINDER_LIKE_WINDOW) | modules.get(y, x);
                    if (x >= FINDER_LIKE_BITS - 1)
                    {
                        if (isFinderLike(rowWindow))
                            result += PENALTY_N3;
                        if (isFinderLike(columnWindow))
                            result += PENALTY_N3;
                    }

                    if (color)
                        dark++;
                }
            }

            // Find smallest k such that (45-5k)% <= dark/total <= (55+5k)%
            const uint32_t total = static_cast<uint32_t>(size) * size;
            for (uint32_t k = 0; dark * BALANCE_SCALE < (BALANCE_LOW - k) * total || dark * BALANCE_SCALE > (BALANCE_HIGH + k) * total; k++)
                result += PENALTY_N4;

            return result;
        }

        // Reed-Solomon generator over GF(2^8/0x11D)

        constexpr uint16_t GF_REDUCTION_POLYNOMIAL = 0x11D;
        constexpr uint8_t GF_GENERATOR             = 0x02;
        constexpr uint8_t GF_HIGH_BIT              = BITS_PER_BYTE - 1;

        // Russian peasant multiplication
        // See: https://en.wikipedia.org/wiki/Ancient_Egyptian_multiplication
        uint8_t gfMultiply(uint8_t x, uint8_t y)
        {
            uint16_t z = 0;
            for (uint8_t i = BITS_PER_BYTE; i-- > 0;)
            {
                z = (z << 1) ^ ((z >> GF_HIGH_BIT) * GF_REDUCTION_POLYNOMIAL);
                z ^= ((y >> i) & 1) * x;
            }
            return z;
        }

        void rsGenerator(uint8_t degree, uint8_t *coefficients)
        {
            memset(coefficients, 0, degree);
            coefficients[degree - 1] = 1;

            // Compute the product polynomial (x - r^0) * (x - r^1) * (x - r^2) * ... * (x - r^{degree-1}),
            // drop the highest term, and store the rest of the coefficients in order of descending powers.
            // Note that r = 0x02, which is a generator element of this field GF(2^8/0x11D).
            uint8_t root = 1;
            for (uint8_t i = 0; i < degree; i++)
            {
                for (uint8_t j = 0; j < degree; j++)
                {
                    coefficients[j] = gfMultiply(coefficients[j], root);
                    if (j + 1 < degree)
                        coefficients[j] ^= coefficients[j + 1];
                }
                root = gfMultiply(root, GF_GENERATOR);
            }
        }

        // Polynomial division remainder, written to result[0], result[stride], ... so the
        // ECC codewords of all blocks land interleaved.
        void rsRemainder(const uint8_t *coefficients, uint8_t degree, const uint8_t *data, uint8_t length, uint8_t *result, uint8_t stride)
        {
            for (uint8_t i = 0; i < length; i++)
            {
                const uint8_t factor = data[i] ^ result[0];
                for (uint8_t j = 1; j < degree; j++)
                    result[(j - 1) * stride] = result[j * stride];
                result[(degree - 1) * stride] = 0;

                for (uint8_t j = 0; j < degree; j++)
                    result[j * stride] ^= gfMultiply(coefficients[j], factor);
            }
        }

        // See: http://www.thonky.com/qr-code-tutorial/structure-final-message
        void addEccAndInterleave(uint8_t version, Ecc ecc, const uint8_t *data, uint8_t *result, uint8_t *coefficients)
        {
            const uint8_t level              = static_cast<uint8_t>(ecc);
            const uint8_t numBlocks          = NUM_ERROR_CORRECTION_BLOCKS[level][version - 1];
            const uint8_t blockEccLength     = NUM_ERROR_CORRECTION_CODEWORDS[level][version - 1] / numBlocks;
            const uint16_t rawCodewords      = NUM_RAW_DATA_MODULES[version - 1] / BITS_PER_BYTE;
            const uint8_t numShortBlocks     = numBlocks - rawCodewords % numBlocks;
            const uint8_t shortBlockDataSize = rawCodewords / numBlocks - blockEccLength;
            const auto blockDataSize         = [=](uint8_t block) -> uint8_t
            { return shortBlockDataSize + (block >= numShortBlocks ? 1 : 0); };

            memset(result, 0, codewordBytes(version));
            rsGenerator(blockEccLength, coefficients);

            uint16_t offset = 0;
            for (uint8_t i = 0; i <= shortBlockDataSize; i++)
            {
                uint16_t blockStart = 0;
                for (uint8_t block = 0; block < numBlocks; block++)
                {
                    if (i < blockDataSize(block))
                        result[offset++] = data[blockStart + i];
                    blockStart += blockDataSize(block);
                }
            }

            const uint8_t *blockData = data;
            for (uint8_t block = 0; block < numBlocks; block++)
            {
                rsRemainder(coefficients, blockEccLength, blockData, blockDataSize(block), &result[offset + block], numBlocks);
                blockData += blockDataSize(block);
            }
        }

        constexpr uint8_t TERMINATOR_BITS = 4;
        constexpr uint8_t PAD_BYTE_FIRST  = 0xEC;
        constexpr uint8_t PAD_BYTE_SECOND = 0x11;
    }

    bool encode(uint8_t version, Ecc ecc, const uint8_t *data, size_t length, const Workspace &workspace)
    {
        const uint8_t size = sizeOf(version);
        memset(workspace.modules, 0, gridBytes(size));

        const uint8_t level         = static_cast<uint8_t>(ecc);
        const uint16_t rawModules   = NUM_RAW_DATA_MODULES[version - 1];
        const uint32_t capacityBits = (rawModules / BITS_PER_BYTE - NUM_ERROR_CORRECTION_CODEWORDS[level][version - 1]) * BITS_PER_BYTE;

        const Mode mode         = selectMode(data, length);
        const uint8_t countBits = charCountBits(specOf(mode), version);
        if (length >= (1UL << countBits))
            return false;
        const uint16_t charCount = static_cast<uint16_t>(length);
        if (segmentBits(specOf(mode), countBits, charCount) > capacityBits)
            return false;

        memset(workspace.codewords, 0, codewordBytes(version));
        BitBuffer codewords{workspace.codewords, 0};
        appendSegment(codewords, mode, countBits, data, charCount);

        // Terminator, byte alignment, then alternating pad bytes up to capacity
        const uint32_t unusedBits = capacityBits - codewords.bitLength;
        codewords.append(0, unusedBits < TERMINATOR_BITS ? unusedBits : TERMINATOR_BITS);
        codewords.append(0, (BITS_PER_BYTE - codewords.bitLength % BITS_PER_BYTE) % BITS_PER_BYTE);
        for (uint8_t pad = PAD_BYTE_FIRST; codewords.bitLength < capacityBits; pad ^= PAD_BYTE_FIRST ^ PAD_BYTE_SECOND)
            codewords.append(pad, BITS_PER_BYTE);

        memset(workspace.isFunction, 0, gridBytes(size));
        Symbol symbol{{workspace.modules, size}, {workspace.isFunction, size}};
        const uint8_t eccFormatBits = ECC_FORMAT_BITS[level];

        drawFunctionPatterns(symbol, version, eccFormatBits);
        addEccAndInterleave(version, ecc, workspace.codewords, workspace.interleaved, workspace.rsCoefficients);
        drawCodewords(symbol, workspace.interleaved, rawModules);

        // Find the best (lowest penalty) mask
        uint8_t bestMask    = 0;
        uint32_t minPenalty = UINT32_MAX;
        for (uint8_t mask = 0; mask < NUM_MASK_PATTERNS; mask++)
        {
            drawFormatBits(symbol, eccFormatBits, mask);
            applyMask(symbol, mask);
            const uint32_t penalty = penaltyScore(symbol.modules);
            if (penalty < minPenalty)
            {
                bestMask   = mask;
                minPenalty = penalty;
            }
            applyMask(symbol, mask); // Undoes the mask due to XOR
        }

        drawFormatBits(symbol, eccFormatBits, bestMask);
        applyMask(symbol, bestMask);
        return true;
    }
}
