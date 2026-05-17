#!/usr/bin/env sh
set -eu

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <input.wav> <output.webp> [title]" >&2
    exit 1
fi

in=$1
out=$2
title=${3:-$(basename "$in")}

command -v cwebp >/dev/null 2>&1 || {
    echo "cwebp (from webp package) is required" >&2
    exit 1
}

mkdir -p "$(dirname "$out")"

png_out="$(dirname "$out")/.tmp-$$.png"

python3 "$(dirname "$0")/render-spectrogram.py" "$in" "$png_out" --title "$title"

cwebp -lossless -quiet "$png_out" -o "$out"
rm -f "$png_out"
