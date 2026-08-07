#!/usr/bin/env bash
# 交叉编译 C++ 核心为 iOS 静态库（设备 arm64 + 模拟器 arm64/x86_64）
set -euo pipefail
cd "$(dirname "$0")"

ROOT="$(pwd)"
CORE="$(cd "$ROOT/../core" && pwd)"
BUILD="$ROOT/build"
mkdir -p "$BUILD/lib/dev" "$BUILD/lib/sim"

# 1) 真机 arm64
cmake -S "$ROOT" -B "$BUILD/ios-device" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT=iphoneos \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
    -DIM_CORE="$CORE" \
    -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO
cmake --build "$BUILD/ios-device" --config Release --target imcore -j 4
cp "$BUILD/ios-device/Release-iphoneos/libimcore.a" "$BUILD/lib/dev/libimcore.a"

# 2) 模拟器 arm64 + x86_64（Xcode 自动产出 fat 库）
cmake -S "$ROOT" -B "$BUILD/ios-sim" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT=iphonesimulator \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
    -DIM_CORE="$CORE" \
    -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO
cmake --build "$BUILD/ios-sim" --config Release --target imcore -j 4
cp "$BUILD/ios-sim/Release-iphonesimulator/libimcore.a" "$BUILD/lib/sim/libimcore.a"

echo "OK:"
echo "  $BUILD/lib/dev/libimcore.a  (真机 arm64)"
echo "  $BUILD/lib/sim/libimcore.a  (模拟器 arm64+x86_64)"
