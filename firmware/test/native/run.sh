#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p build/native
c++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Ilib/hermes_security/src -Itest/native/stubs -Iinclude -Isrc \
  lib/hermes_security/src/*.cpp src/ptt_controller.cpp test/native/security_test.cpp \
  -lcrypto -o build/native/security_test
./build/native/security_test
