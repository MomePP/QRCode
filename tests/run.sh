#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")"

clang++ -std=c++20 -Wall -Wextra -Werror -O2 \
    run-tests.cpp QrCode.cpp QrSegment.cpp BitBuffer.cpp ../src/QRCode.cpp \
    -o test
./test
