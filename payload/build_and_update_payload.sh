#!/bin/bash

set -xe

mkdir -p tmp
./gradlew build
cp app/build/outputs/apk/debug/app-debug.apk tmp/
pushd tmp
unzip -o app-debug.apk classes3.dex
cp classes3.dex payload_dex_data
xxd -i payload_dex_data > ../../app/app/src/main/cpp/payload_dex_data.hpp
popd
