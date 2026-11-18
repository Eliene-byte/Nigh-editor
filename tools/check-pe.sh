#!/usr/bin/env bash
# Audit PE headers and imports, not a replacement for booting real XP SP3.
set -euo pipefail
export LC_ALL=C
bin="${1:-build}"
for file in "$bin/NighEditor.exe" "$bin/codecs/ffmpeg.exe" "$bin/codecs/ffprobe.exe"; do
    info=$(objdump -p "$file")
    for pattern in 'file format pei-i386' 'MajorOSystemVersion[[:space:]]+5$' 'MinorOSystemVersion[[:space:]]+1$' \
        'MajorSubsystemVersion[[:space:]]+5$' 'MinorSubsystemVersion[[:space:]]+1$'; do
        grep -Eq "$pattern" <<< "$info" || { echo "PE x86 / 5.01 esperado: $file" >&2; exit 1; }
    done
    while read -r lib; do
        case "${lib,,}" in
            kernel32.dll|msvcrt.dll|user32.dll|gdi32.dll|shell32.dll|comdlg32.dll|winmm.dll|advapi32.dll|psapi.dll) ;;
            *) echo "DLL fora do perfil XP: $lib ($file)" >&2; exit 1;;
        esac
    done < <(awk '/DLL Name:/ {print $3}' <<< "$info")
    imports=$(sed '/PE File Base Relocations/,$d' <<< "$info")
    if grep -Ei '(GetTickCount64|InitializeCriticalSectionEx|GetSystemTimePreciseAsFileTime|AcquireSRWLock|ReleaseSRWLock|InitOnce|FlsAlloc|GetFileInformationByHandleEx|GetDpiForWindow|GetLocaleInfoEx|SetThreadErrorMode|GetFinalPathNameByHandle)' <<< "$imports"; then
        echo "Importação posterior ao XP: $file" >&2; exit 1
    fi
    printf 'OK · PE32 / subsistema 5.01 · %s\n' "$file"
done
