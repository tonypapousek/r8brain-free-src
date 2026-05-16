#!/usr/bin/env sh
set -eu

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <output.wav> [duration_seconds]" >&2
    exit 1
fi

out=$1
duration=${2:-8}

command -v python3 >/dev/null 2>&1 || {
    echo "python3 is required" >&2
    exit 1
}

mkdir -p "$(dirname "$out")"

python3 -c "
import numpy as np
from scipy.io import wavfile
from scipy.signal import chirp

sr = 192000
dur = $duration
t = np.arange(int(sr * dur)) / sr
x = 0.5 * chirp(t, f0=20.0, f1=90000.0, t1=dur, method='logarithmic')
wavfile.write('$out', sr, x.astype(np.float32))
"
