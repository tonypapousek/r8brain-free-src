#!/usr/bin/env sh
set -eu

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <input.wav> <output.png> [title]" >&2
    exit 1
fi

in=$1
out=$2
title=${3:-$(basename "$in")}

mkdir -p "$(dirname "$out")"

python3 "$(dirname "$0")/render-spectrogram.py" "$in" "$out" --title "$title"
