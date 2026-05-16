#!/usr/bin/env sh
set -eu

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <input.wav> <output.png> [title]" >&2
    exit 1
fi

in=$1
out=$2
title=${3:-$(basename "$in")}

command -v sox >/dev/null 2>&1 || {
    echo "sox is required" >&2
    exit 1
}

mkdir -p "$(dirname "$out")"

sox "$in" -n spectrogram -x 1200 -y 513 -z 100 -q 6 -w Dolph -W 10 -t "$title" -o "$out"
