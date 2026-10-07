/**
 *  QRCode
 *
 *  A quick example of generating a QR code.
 *
 *  This prints the QR code to the serial monitor as solid blocks. Each module
 *  is two characters wide, since the monospace font used in the serial monitor
 *  is approximately twice as tall as wide.
 */

#include <QRCode.h>

constexpr uint32_t BAUD_RATE  = 115200;
constexpr uint8_t QUIET_ZONE = 4;

qrcode::QRCode<3> qr;

void printQuietRows()
{
    for (uint8_t row = 0; row < QUIET_ZONE; row++)
        Serial.print("\n");
}

void setup()
{
    Serial.begin(BAUD_RATE);

    const uint32_t start = millis();
    const bool encoded   = qr.encode("HELLO WORLD", qrcode::Ecc::L);
    const uint32_t dt    = millis() - start;

    if (!encoded)
    {
        Serial.print("Data does not fit a version 3 QR code at this ECC level\n");
        return;
    }

    Serial.print("QR Code Generation Time: ");
    Serial.print(dt);
    Serial.print("\n");

    printQuietRows();
    for (uint8_t y = 0; y < qr.SIZE; y++)
    {
        for (uint8_t column = 0; column < QUIET_ZONE; column++)
            Serial.print("  ");

        // UTF-8 \u2588 is a solid block
        for (uint8_t x = 0; x < qr.SIZE; x++)
            Serial.print(qr.module(x, y) ? "\u2588\u2588" : "  ");

        Serial.print("\n");
    }
    printQuietRows();
}

void loop()
{
}
