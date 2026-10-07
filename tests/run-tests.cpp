#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../src/QRCode.h"
#include "QrCode.hpp"
#include "QrSegment.hpp"

namespace
{
    using Nayuki = qrcodegen::QrCode;

    constexpr int QUIET_ZONE              = 4;
    constexpr size_t CAPACITY_PROBE_LIMIT = 8192;
    constexpr size_t OVERSIZED_LENGTH     = 65546;

    constexpr qrcode::Ecc ECC_LEVELS[] = {qrcode::Ecc::L, qrcode::Ecc::M, qrcode::Ecc::Q, qrcode::Ecc::H};

    const char *const WIFI_PAYLOAD         = "WIFI:T:WPA;S:GoGo-ABCD;P:12345678;;";
    const char *const NUMERIC_PATTERN      = "0123456789";
    const char *const ALPHANUMERIC_PATTERN = "GOGO BOARD $%*+-./:0123456789";
    const char *const BYTE_PATTERN         = "gogo-board;";
    const std::vector<uint8_t> BINARY_DATA = {0x00, 0xFF, 0x80, 'A', 0x7F, '\n'};

    struct Stats
    {
        int passed = 0;
        int failed = 0;
    };

    const Nayuki::Ecc &nayukiEcc(qrcode::Ecc ecc)
    {
        switch (ecc)
        {
        case qrcode::Ecc::L:
            return Nayuki::Ecc::LOW;
        case qrcode::Ecc::M:
            return Nayuki::Ecc::MEDIUM;
        case qrcode::Ecc::Q:
            return Nayuki::Ecc::QUARTILE;
        case qrcode::Ecc::H:
            break;
        }
        return Nayuki::Ecc::HIGH;
    }

    char eccName(qrcode::Ecc ecc)
    {
        return "LMQH"[static_cast<uint8_t>(ecc)];
    }

    std::optional<Nayuki> nayukiText(const std::string &text, int version, qrcode::Ecc ecc)
    {
        try
        {
            return Nayuki::encodeText(text.c_str(), version, nayukiEcc(ecc));
        }
        catch (const char *)
        {
            return std::nullopt;
        }
    }

    std::optional<Nayuki> nayukiBytes(const std::vector<uint8_t> &data, int version, qrcode::Ecc ecc)
    {
        try
        {
            return Nayuki::encodeSegments({qrcodegen::QrSegment::makeBytes(data)}, nayukiEcc(ecc), version, version, -1, false);
        }
        catch (const char *)
        {
            return std::nullopt;
        }
    }

    std::string repeatTo(const std::string &pattern, size_t length)
    {
        std::string text;
        while (text.size() < length)
            text += pattern;
        text.resize(length);
        return text;
    }

    // Largest length of `pattern` Nayuki fits into (version, ecc), found by bisection on its "Data too long" throw.
    size_t nayukiCapacity(const std::string &pattern, int version, qrcode::Ecc ecc)
    {
        size_t fits = 1, overflows = CAPACITY_PROBE_LIMIT;
        while (overflows - fits > 1)
        {
            const size_t middle = (fits + overflows) / 2;
            if (nayukiText(repeatTo(pattern, middle), version, ecc))
                fits = middle;
            else
                overflows = middle;
        }
        return fits;
    }

    template <uint8_t Version>
    int mismatchedModules(const qrcode::QRCode<Version> &code, const Nayuki &expected)
    {
        int wrong = 0;
        for (int y = -QUIET_ZONE; y < expected.size + QUIET_ZONE; y++)
            for (int x = -QUIET_ZONE; x < expected.size + QUIET_ZONE; x++)
                if ((expected.getModule(x, y) != 0) != code.module(static_cast<uint8_t>(x), static_cast<uint8_t>(y)))
                    wrong++;
        return wrong;
    }

    template <uint8_t Version>
    int darkModules(const qrcode::QRCode<Version> &code)
    {
        int dark = 0;
        for (int y = 0; y < qrcode::sizeOf(Version); y++)
            for (int x = 0; x < qrcode::sizeOf(Version); x++)
                dark += code.module(x, y);
        return dark;
    }

    void record(Stats &stats, bool ok, int version, qrcode::Ecc ecc, const char *what, size_t length, const char *detail)
    {
        if (ok)
        {
            stats.passed++;
            return;
        }
        stats.failed++;
        printf("FAIL version=%d ecc=%c case=%s length=%zu: %s\n", version, eccName(ecc), what, length, detail);
    }

    // Encodes into `code` and checks it against Nayuki: identical modules when Nayuki fits, otherwise false and blank.
    template <uint8_t Version>
    void expectNayuki(Stats &stats, qrcode::QRCode<Version> &code, qrcode::Ecc ecc, const char *what, size_t length, bool encoded, const std::optional<Nayuki> &expected)
    {
        if (expected)
        {
            const int wrong = encoded ? mismatchedModules(code, *expected) : -1;
            record(stats, wrong == 0, Version, ecc, what, length, encoded ? "modules differ from Nayuki" : "encode returned false but Nayuki fits");
        }
        else
            record(stats, !encoded && darkModules(code) == 0, Version, ecc, what, length, "oversized data must return false and clear the grid");
    }

    template <uint8_t Version>
    void expectText(Stats &stats, qrcode::QRCode<Version> &code, qrcode::Ecc ecc, const char *what, const std::string &text)
    {
        const bool encoded = code.encode(text.c_str(), ecc);
        expectNayuki(stats, code, ecc, what, text.size(), encoded, nayukiText(text, Version, ecc));
    }

    template <uint8_t Version>
    void testVersion(Stats &stats)
    {
        for (const qrcode::Ecc ecc : ECC_LEVELS)
        {
            qrcode::QRCode<Version> code;

            for (const char *text : {"HELLO", "Hello", "1234", WIFI_PAYLOAD})
                expectText(stats, code, ecc, text, text);

            const bool encoded = code.encode(BINARY_DATA.data(), static_cast<uint16_t>(BINARY_DATA.size()), ecc);
            expectNayuki(stats, code, ecc, "binary", BINARY_DATA.size(), encoded, nayukiBytes(BINARY_DATA, Version, ecc));

            for (const char *pattern : {NUMERIC_PATTERN, ALPHANUMERIC_PATTERN, BYTE_PATTERN})
            {
                const size_t capacity = nayukiCapacity(pattern, Version, ecc);
                // The capacity encode leaves a symbol behind, so the overflow must actively clear it.
                expectText(stats, code, ecc, "capacity", repeatTo(pattern, capacity));
                expectText(stats, code, ecc, "capacity+1", repeatTo(pattern, capacity + 1));
            }
        }
    }

    template <uint8_t... Indices>
    void testAllVersions(Stats &stats, std::integer_sequence<uint8_t, Indices...>)
    {
        (testVersion<qrcode::MIN_VERSION + Indices>(stats), ...);
    }

    // strlen beyond the 16-bit length must not wrap into a short payload that fits.
    void testOversizedText(Stats &stats)
    {
        qrcode::QRCode<qrcode::MAX_VERSION> code;
        const bool primed      = code.encode("HELLO", qrcode::Ecc::L);
        const std::string text = repeatTo(NUMERIC_PATTERN, OVERSIZED_LENGTH);
        const bool encoded     = code.encode(text.c_str(), qrcode::Ecc::L);
        record(stats, primed && !encoded && darkModules(code) == 0, qrcode::MAX_VERSION, qrcode::Ecc::L, "oversized", text.size(), "text longer than 65535 must return false and clear the grid");
    }
}

int main()
{
    Stats stats;
    testAllVersions(stats, std::make_integer_sequence<uint8_t, qrcode::MAX_VERSION - qrcode::MIN_VERSION + 1>{});
    testOversizedText(stats);

    printf("Tests complete: %d passed, %d failed (out of %d)\n", stats.passed, stats.failed, stats.passed + stats.failed);
    return stats.failed == 0 ? 0 : 1;
}
