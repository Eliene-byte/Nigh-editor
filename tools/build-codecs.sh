#!/usr/bin/env bash
# Build the standalone, local-files-only media tools. No FFmpeg code links into Nigh Editor.
set -euo pipefail
export LC_ALL=C
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
sources="${SOURCE_DIR:-$root/.cache/sources}"
out="${BIN_DIR:-$root/build/codecs}"
jobs="${JOBS:-4}"
triplet="${CROSS:-i686-w64-mingw32}"
default_work="$root/.cache/codecs"
if [[ "$triplet" != i686-w64-mingw32 ]]; then default_work+="-$triplet"; fi
work="${WORK_DIR:-$default_work}"
x264_commit=31e19f92f00c7003fa115047ce50978bc98c3a0d
ffmpeg_tag=n4.4.8
mkdir -p "$work" "$sources" "$out"
work=$(cd -- "$work" && pwd)
sources=$(cd -- "$sources" && pwd)
out=$(cd -- "$out" && pwd)

fetch() {
    if [[ ! -f "$sources/$2" ]]; then
        curl --fail --location --retry 2 "$1" -o "$sources/$2.part"
        mv -- "$sources/$2.part" "$sources/$2"
    fi
}
fetch "https://codeload.github.com/FFmpeg/FFmpeg/tar.gz/refs/tags/$ffmpeg_tag" ffmpeg-4.4.8.tar.gz
fetch "https://codeload.github.com/mirror/x264/tar.gz/$x264_commit" x264-31e19f9.tar.gz
(cd "$sources" && sha256sum -c "$root/tools/codec-sources.sha256")

for pair in "x264 x264-31e19f9.tar.gz" "ffmpeg ffmpeg-4.4.8.tar.gz"; do
    read -r dir archive <<< "$pair"
    if [[ ! -f "$work/$dir/configure" ]]; then
        mkdir -p "$work/$dir"
        tar xf "$sources/$archive" -C "$work/$dir" --strip-components=1
    fi
done

cross=() x264_cross=() xp_flags=() media_suffix="" x264_env=(env)
if [[ "$triplet" != host ]]; then
    command -v "$triplet-gcc" >/dev/null || { echo "Instale o compilador $triplet-gcc." >&2; exit 1; }
    cross=(--arch=x86 --target-os=mingw32 "--cross-prefix=$triplet-" --enable-cross-compile
        --extra-ldflags=-Wl,--major-os-version,5,--minor-os-version,1,--major-subsystem-version,5,--minor-subsystem-version,1)
    x264_cross=("--host=$triplet" "--cross-prefix=$triplet-")
    xp_flags=(-march=pentium-m -D_WIN32_WINNT=0x0501 -DWINVER=0x0501)
    media_suffix=.exe
    x264_env=(env "AR=$triplet-ar" "RANLIB=$triplet-ranlib")
fi
prefix="$work/prefix"
(
    cd "$work/x264"
    if [[ ! -f "$prefix/lib/libx264.a" ]]; then
        "${x264_env[@]}" ./configure "${x264_cross[@]}" --prefix="$prefix" --enable-static --disable-cli \
            --bit-depth=8 --chroma-format=420 --disable-asm --disable-thread --disable-opencl --extra-cflags="${xp_flags[*]}"
        make -j "$jobs"
        make install
    fi
)

(
    cd "$work/ffmpeg"
    if [[ ! -f ffbuild/config.mak ]]; then
        PKG_CONFIG_PATH="$prefix/lib/pkgconfig" ./configure "${cross[@]}" \
            --enable-gpl --enable-libx264 --enable-static --disable-shared --disable-autodetect \
            --disable-debug --disable-doc --enable-small --disable-everything --disable-network \
            --disable-avdevice --disable-postproc --disable-ffplay --disable-hwaccels \
            --disable-w32threads --disable-pthreads --disable-os2threads --disable-x86asm \
            --extra-cflags="${xp_flags[*]} -I$prefix/include" --extra-libs="-L$prefix/lib -lx264" \
            --enable-protocol=file,pipe \
            --enable-demuxer=mov,matroska,avi,mpegts,mpegps,asf,flv,concat,dv,ogg,wav,mp3,aac,flac,pcm_s16le,rawvideo \
            --enable-muxer=mp4,avi,pcm_s16le,null \
            --enable-parser=h264,hevc,mpeg4video,mpegvideo,aac,aac_latm,mpegaudio,ac3,flac,opus,vp8,vp9,h263,vc1,mjpeg,vorbis \
            --enable-decoder=h264,hevc,mpeg4,mpeg2video,mpeg1video,vp8,vp9,mjpeg,dvvideo,h263,wmv1,wmv2,wmv3,vc1,rawvideo,prores,vp6f,flv,aac,aac_latm,mp3,mp2,ac3,flac,vorbis,opus,pcm_s16le,pcm_s16be,pcm_s24le,pcm_s24be,pcm_s32le,pcm_s32be,pcm_u8,pcm_f32le,pcm_f32be,pcm_alaw,pcm_mulaw,adpcm_ima_wav,adpcm_ms,wmav1,wmav2,alac \
            --enable-encoder=libx264,aac,mjpeg,pcm_s16le \
            --enable-filter=scale,pad,setsar,fps,tpad,format,aresample,aformat,apad,atrim,trim,anull,null,transpose,hflip,vflip,setpts,asetpts,color,rotate \
            --enable-bsf=aac_adtstoasc,h264_mp4toannexb,mpeg4_unpack_bframes
    fi
    make -j "$jobs" "ffmpeg$media_suffix" "ffprobe$media_suffix"
    cp -- "ffmpeg$media_suffix" "ffprobe$media_suffix" "$out/"
)
