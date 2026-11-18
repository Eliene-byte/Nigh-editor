#!/usr/bin/env bash
# Linux x86_64: prepare the exact legacy MSVCRT toolchain used for the portable build.
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
cache="$root/.cache/cross"
commit=9d126b39eeeb93509c189b09aff3c53b7a99eb1b
sha=103f668fd80c074fbb0455bfdd2f0a771d8ee5185501931b0204ccc64345e474
mkdir -p "$cache"
archive="$cache/mingw-pinned.tar.gz"
if [[ ! -f "$archive" ]]; then
    curl --fail --location --retry 2 \
        "https://codeload.github.com/crystax/android-platform-prebuilts-gcc-linux-x86-host-i686-w64-mingw32/tar.gz/$commit" \
        -o "$archive.part"
    mv -- "$archive.part" "$archive"
fi
printf '%s  %s\n' "$sha" "$archive" | sha256sum -c - >&2
if [[ ! -x "$cache/aosp/bin/i686-w64-mingw32-g++" ]]; then
    mkdir -p "$cache/aosp"
    tar xf "$archive" -C "$cache/aosp" --strip-components=1
fi
printf '%s\n' "$cache/aosp/bin"
