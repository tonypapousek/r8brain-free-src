#!/usr/bin/env sh
set -eu

usage() {
    echo "Usage: $0 <output.wav> [--rate <sample_rate>] [duration_seconds]" >&2
    exit 1
}

if [ "$#" -lt 1 ]; then usage; fi

out=$1
shift

rate=192000
while [ "$#" -gt 0 ]; do
    case "$1" in
        --rate) rate="$2"; shift 2 ;;
        *) duration="$1"; shift ;;
    esac
done
duration=${duration:-8}

command -v python3 >/dev/null 2>&1 || {
    echo "python3 is required" >&2
    exit 1
}

mkdir -p "$(dirname "$out")"

python3 -c "
import numpy as np
from scipy.io import wavfile
from scipy.signal import chirp

sr = $rate
dur = $duration
f_end = int(sr * 0.46875)
t = np.arange(int(sr * dur)) / sr
x = 0.5 * chirp(t, f0=20.0, f1=float(f_end), t1=dur, method='logarithmic')
wavfile.write('$out', sr, x.astype(np.float32))
"
