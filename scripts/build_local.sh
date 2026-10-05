#!/usr/bin/env bash
# Build/test shared state, configuration and replay without OpenCV or Jetson.
set -euo pipefail
cd "$(dirname "$0")/.."
coco_cmake="${CMAKE_BIN:-cmake}"
coco_ctest="${CTEST_BIN:-$(dirname "$(command -v "$coco_cmake")")/ctest}"
coco_build_dir="${1:-build/local}"
coco_compiler_options=()
# Select installed Command Line Tools only for this process; do not change xcode-select.
if [[ "$(uname -s)" == Darwin && -z "${DEVELOPER_DIR:-}" &&
      -x /Library/Developer/CommandLineTools/usr/bin/clang++ ]]; then
  export DEVELOPER_DIR=/Library/Developer/CommandLineTools
  coco_compiler_options=(-DCMAKE_CXX_COMPILER="$DEVELOPER_DIR/usr/bin/clang++"
                         -DCMAKE_OSX_SYSROOT="$DEVELOPER_DIR/SDKs/MacOSX.sdk")
fi
"$coco_cmake" -S . -B "$coco_build_dir" -DCOCO_BUILD_RUNTIME=OFF \
  -DCMAKE_BUILD_TYPE=Debug "${coco_compiler_options[@]}"
"$coco_cmake" --build "$coco_build_dir" -j2
"$coco_ctest" --test-dir "$coco_build_dir" --output-on-failure
