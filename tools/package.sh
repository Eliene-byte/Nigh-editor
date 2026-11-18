#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
bin="${BIN_DIR:-build}"
source_dir="${SOURCE_DIR:-$root/.cache/sources}"
artifact_dir="${ARTIFACT_DIR:-$root/artifacts}"
mkdir -p "$artifact_dir"
artifact_dir=$(cd -- "$artifact_dir" && pwd)
cd "$root"
./tools/check-pe.sh "$bin"
(cd "$source_dir" && sha256sum -c "$root/tools/codec-sources.sha256")

stage=$(mktemp -d "${TMPDIR:-/tmp}/nigh-release.XXXXXXXX")
trap 'rm -rf -- "$stage"' EXIT
name='Nigh Editor'
portable="$stage/$name"
license="$portable/Licencas"
code="$license/Codigo-fonte-dos-codecs"
mkdir -p "$portable/demo" "$code/tools"
cp "$bin/NighEditor.exe" "$bin/codecs/ffmpeg.exe" "$bin/codecs/ffprobe.exe" "$portable/"
cp demo/*.mp4 demo/*.m4a "$portable/demo/"
cp tools/build-codecs.sh tools/codec-sources.sha256 "$code/tools/"
cp "$source_dir/ffmpeg-4.4.8.tar.gz" "$source_dir/x264-31e19f9.tar.gz" "$code/"
tar xOf "$code/ffmpeg-4.4.8.tar.gz" FFmpeg-n4.4.8/COPYING.GPLv2 > "$license/FFmpeg-GPLv2.txt"
tar xOf "$code/x264-31e19f9.tar.gz" x264-31e19f92f00c7003fa115047ce50978bc98c3a0d/COPYING > "$license/x264-GPLv2.txt"
sed -n '/^ALTERNATIVE A - MIT License/,$p' src/vendor/stb_image.h | sed '$d' > "$license/stb_image.txt"
cat > "$license/AVISOS.txt" <<'NOTICE'
FFmpeg 4.4.8 e x264 (commit 31e19f92f00c7003fa115047ce50978bc98c3a0d)
--------------------------------------------------------------------
ffmpeg.exe e ffprobe.exe são programas independentes, compilados com
--enable-gpl e libx264, distribuídos sob os termos da GNU GPL versão 2
ou posterior. Os arquivos de licença e o código-fonte correspondente
estão nesta pasta. Nigh Editor executa esses programas como processos
separados. Não se comunica com serviços remotos.

Para recompilar os codecs no Linux, copie Codigo-fonte-dos-codecs para um
caminho sem espaços e deixe o MinGW-w64 i686/MSVCRT no PATH:
    cd Codigo-fonte-dos-codecs
    SOURCE_DIR="$PWD" bash tools/build-codecs.sh

Código a montante:
https://github.com/FFmpeg/FFmpeg/tree/n4.4.8
https://github.com/mirror/x264/tree/31e19f92f00c7003fa115047ce50978bc98c3a0d

A decodificação das prévias no Nigh Editor usa stb_image (Sean Barrett e
contribuidores; licença MIT/domínio público). Leia stb_image.txt.
https://github.com/nothings/stb
NOTICE
python3 - "$portable/LEIA-ME.txt" <<'PY'
import pathlib, sys
text = pathlib.Path('tools/LEIA-ME.txt').read_text(encoding='utf-8')
pathlib.Path(sys.argv[1]).write_bytes(('\ufeff'+text.replace('\n','\r\n')).encode('utf-8'))
PY
bundle='Nigh-Editor-0.2.0-Windows-x86.zip'
(cd "$stage" && zip -q -9 -r "$stage/$bundle" "$name")
mv -f -- "$stage/$bundle" "$artifact_dir/$bundle"
(cd "$artifact_dir" && sha256sum "$bundle" > "$bundle.sha256")
printf '%s\n' "$artifact_dir/$bundle"
