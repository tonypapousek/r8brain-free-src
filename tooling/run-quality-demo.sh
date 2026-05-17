#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
WORK="$ROOT/tmp/quality"
IMG="$ROOT/doc/img"
SRC="$WORK/input/sweep-192k.wav"

make -C "$ROOT"

rm -rf "$WORK"
mkdir -p "$WORK/input" "$IMG"

"$ROOT/tooling/make-sweep.sh" "$SRC"

for rate in 44100 48000 96000; do
    ours_dir="$WORK/resampler-$rate"
    ours_wav="$ours_dir/sweep-192k.wav"

    mkdir -p "$ours_dir"

    "$ROOT/dist/resampler" -i "$WORK/input" -o "$ours_dir" -r "$rate" -b float

    "$ROOT/tooling/make-spectrogram.sh" "$ours_wav" "$IMG/resampler-$rate.png" "resampler: 192 kHz sweep -> $rate Hz"
done

"$ROOT/tooling/analyze-quality.py" \
    "$WORK/resampler-44100/sweep-192k.wav" \
    "$WORK/resampler-48000/sweep-192k.wav" \
    "$WORK/resampler-96000/sweep-192k.wav"

echo "Wrote quality images to doc/img/"
