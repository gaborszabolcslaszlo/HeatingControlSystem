#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_build_dir="${TMPDIR:-/tmp}/heating-boiler-tests"
mkdir -p "$test_build_dir"
g++ -std=c++11 -g -fsanitize=address,undefined -Wno-deprecated-declarations \
  -DUNIT_TESTING '-DlogInfo(...)=((void)0)' '-DlogDebug(...)=((void)0)' \
  '-DlogWarn(...)=((void)0)' '-DlogError(...)=((void)0)' \
  -I tests/native -I src -I .pio/libdeps/esp32wroom/ArduinoJson/src \
  tests/native/boiler_buffer_operations.cpp \
  src/Controllers/pid.cpp src/HeatingSystemElements/HeatingController.cpp \
  src/HeatingSystemElements/Kazan.cpp src/HeatingSystemElements/Puffer.cpp \
  src/HeatingSystemElements/Radiator.cpp src/HeatingSystemElements/HeatingElement.cpp \
  src/HeatingSystemElements/HeatingTransfer.cpp src/HeatingSystemElements/Sensor.cpp \
  src/HeatingSystemElements/pump.cpp src/HeatingSystemElements/valve.cpp \
  src/HeatingSystemElements/MessageBus.cpp -o "$test_build_dir/boiler-test"
"$test_build_dir/boiler-test"
