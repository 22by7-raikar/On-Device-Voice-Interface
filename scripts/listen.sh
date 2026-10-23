#!/usr/bin/env bash
# Feed a WAV file or a live microphone to odvi_listen.
#
#   scripts/listen.sh --model models/ggml-base.en.bin --wav speech.wav [--realtime]
#   scripts/listen.sh --model models/ggml-base.en.bin --mic [alsa-device]
#
# Audio is converted to mono 16 kHz S16LE with ffmpeg. Transcript events (JSON
# Lines) go to stdout; diagnostics go to stderr.

set -euo pipefail

usage() {
    sed -n '2,8p' "$0" >&2
    exit 2
}

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
listen="${ODVI_LISTEN:-$repo_root/build/release/apps/odvi_listen}"
model="" wav="" mic="" realtime=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --model) model="${2:?}"; shift 2 ;;
        --wav) wav="${2:?}"; shift 2 ;;
        --mic)
            if [[ $# -ge 2 && "$2" != --* ]]; then mic="$2"; shift 2; else mic="default"; shift; fi ;;
        --realtime) realtime=1; shift ;;
        *) usage ;;
    esac
done
[[ -n "$model" && ( -n "$wav" || -n "$mic" ) ]] || usage
[[ -x "$listen" ]] || {
    echo "error: build first: cmake --preset release && cmake --build --preset release" >&2
    exit 1
}
command -v ffmpeg >/dev/null || { echo "error: ffmpeg is required" >&2; exit 1; }

if [[ -n "$wav" ]]; then
    source_args=(-i "$wav")
    if [[ "$realtime" -eq 1 ]]; then source_args=(-re "${source_args[@]}"); fi
    device="file" transport="wav_replay"
else
    source_args=(-f alsa -i "$mic")
    device="alsa:$mic" transport="alsa_pcm"
fi

ffmpeg -hide_banner -loglevel error "${source_args[@]}" -ar 16000 -ac 1 -f s16le - |
    "$listen" --rate 16000 --channels 1 --model "$model" \
        --capture-device "$device" --transport "$transport"
