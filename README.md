QRCode
======

A [QR code](https://en.wikipedia.org/wiki/QR_code) generator for memory-constrained
C++ targets, forked from [ricmoo/QRCode](https://github.com/ricmoo/QRCode) for the
GoGo Board firmware (ESP32-S3).

- The QR version is a template parameter, so the module grid and every scratch
  buffer have fixed sizes known at compile time. No heap, no variable-length arrays.
- The mode (numeric, alphanumeric or byte) is chosen automatically, and the mask
  is chosen by penalty score. Output is bit-identical to the original library.
- MIT License.


API
---

```cpp
#include <QRCode.h>

qrcode::QRCode<3> qr; // version 3: 29 x 29 modules

if (qr.encode("HELLO WORLD", qrcode::Ecc::L))
{
    for (uint8_t y = 0; y < qr.SIZE; y++)
        for (uint8_t x = 0; x < qr.SIZE; x++)
            drawModule(x, y, qr.module(x, y)); // true = dark
}
```

| Member | Meaning |
|---|---|
| `QRCode<Version>` | Version 1 to 40. Holds the module grid (`(SIZE * SIZE + 7) / 8` bytes). |
| `SIZE` | Modules per side, `4 * Version + 17`. Also `qrcode::sizeOf(version)`. |
| `encode(const char *text, Ecc ecc)` | Encodes `strlen(text)` bytes. |
| `encode(const uint8_t *data, uint16_t length, Ecc ecc)` | Encodes raw bytes. |
| `module(x, y)` | Whether the module is dark. `false` outside `0..SIZE-1`, so a quiet zone can be read without bounds checks. |
| `Ecc::L`, `M`, `Q`, `H` | Error correction levels, recovering about 7%, 15%, 25% and 30% damage. |

`encode` runs on the caller's stack. For version 7 its scratch buffers total
676 bytes, and its frame measures 736 bytes on ESP32-S3 with `-Os`.


Error contract
--------------

`encode` returns `false` when the data does not fit `Version` at the requested
error correction level, including a text longer than the character-count field
allows. It never writes out of bounds. On `false` the grid is cleared, so
`module()` reads all light and a caller cannot draw a stale symbol.

The capacity of each version, level and mode is listed in `generate_table.py`.


Tests
-----

```sh
tests/run.sh
```

The tests build with `clang++ -std=c++20 -Wall -Wextra -Werror` on the host and
compare every version and error correction level against
[Project Nayuki's library](https://github.com/nayuki/QR-Code-generator/tree/master/cpp)
in `tests/`. Each version is checked with fixed inputs, numeric, alphanumeric and
byte payloads at exactly their capacity, and capacity + 1, which must return
`false` and leave a blank grid.


Credits
-------

Written by Richard Moore, with major parts derived from
[Project Nayuki's QR code library](https://www.nayuki.io/page/qr-code-generator-library),
which was also critical for testing. Refactored to the C++ API by MomePP.
