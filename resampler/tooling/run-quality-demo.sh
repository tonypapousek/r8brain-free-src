#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
RES="$ROOT/resampler"
WORK="$ROOT/tmp/quality"
IMG="$RES/doc/img"

make -C "$ROOT"

rm -rf "$WORK"
mkdir -p "$WORK/input" "$IMG"

# Generate source sweeps at different sample rates
for rate in 192000 44100 96000; do
    "$RES/tooling/make-sweep.sh" "$WORK/input/sweep-${rate}.wav" --rate "$rate"
done

process() {
    src=$1 dst=$2
    input_dir="$WORK/input-${src}"
    output_dir="$WORK/resampler-${src}-${dst}"

    mkdir -p "$input_dir" "$output_dir"
    cp "$WORK/input/sweep-${src}.wav" "$input_dir/"

    "$ROOT/dist/resampler" \
        -i "$input_dir" \
        -o "$output_dir" \
        -r "$dst" -b float

    title="${src} Hz -> ${dst} Hz"
    "$RES/tooling/make-spectrogram.sh" \
        "$output_dir/sweep-${src}.wav" \
        "$IMG/resampler-${src}-${dst}.webp" \
        "$title"
}

# Downsampling (3)
process 192000 44100
process 192000 48000
process 192000 96000
# Upsampling (3)
process 44100 48000
process 44100 96000
process 96000 192000

# Analyze all outputs
analyze() {
    src=$1 dst=$2
    "$RES/tooling/analyze-quality.py" \
        "$WORK/resampler-${src}-${dst}/sweep-${src}.wav" \
        --target-rate "$dst" \
        --src-rate "$src"
}
analyze 192000 44100
analyze 192000 48000
analyze 192000 96000
analyze 44100 48000
analyze 44100 96000
analyze 96000 192000

echo "Wrote quality images to resampler/doc/img/"
