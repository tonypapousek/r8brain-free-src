#!/usr/bin/env python3
import argparse
import math
from pathlib import Path

import numpy as np
from scipy.io import wavfile
from scipy.signal import stft


def read_wav(path):
    sample_rate, data = wavfile.read(path)
    if data.ndim > 1:
        data = data[:, 0]

    if np.issubdtype(data.dtype, np.integer):
        data = data.astype(np.float64) / np.iinfo(data.dtype).max
    else:
        data = data.astype(np.float64)

    return sample_rate, data


def rms_db(samples):
    if len(samples) == 0:
        return float("-inf")

    rms = np.sqrt(np.mean(samples * samples))
    return 20.0 * math.log10(max(rms, 1e-300))


def check_output(path, target_rate, args):
    sample_rate, samples = read_wav(path)
    if sample_rate != target_rate:
        raise RuntimeError(f"{path}: expected {target_rate} Hz, got {sample_rate} Hz")

    cutoff = args.duration * math.log((target_rate / 2.0) / args.start_freq) / math.log(args.end_freq / args.start_freq)
    pre_start = max(0, int((cutoff - args.pre_window) * sample_rate))
    pre_end = max(pre_start, int((cutoff - args.guard) * sample_rate))
    post_start = min(len(samples), int((cutoff + args.guard) * sample_rate))

    pre_db = rms_db(samples[pre_start:pre_end])
    post_db = rms_db(samples[post_start:])
    rejection_db = pre_db - post_db

    freqs, times, spectrum = stft(
        samples,
        fs=sample_rate,
        window="hann",
        nperseg=args.fft_size,
        noverlap=args.fft_size // 2,
        boundary=None,
        padded=False,
    )
    db = 20.0 * np.log10(np.abs(spectrum) + 1e-300)
    main_peaks = []
    offtrack_peaks = []
    post_peaks = []

    for index, time in enumerate(times):
        expected_freq = args.start_freq * ((args.end_freq / args.start_freq) ** (time / args.duration))
        column = db[:, index]

        if expected_freq < sample_rate / 2.0:
            track = np.abs(freqs - expected_freq) <= args.track_width_hz
            if np.any(track):
                main_peaks.append(float(np.max(column[track])))
                offtrack_peaks.append(float(np.max(column[~track])))
        elif time >= cutoff + args.guard:
            post_peaks.append(float(np.max(column)))

    main_db = float(np.median(main_peaks)) if main_peaks else float("-inf")
    offtrack_db = max(offtrack_peaks) if offtrack_peaks else float("-inf")
    post_peak_db = max(post_peaks) if post_peaks else float("-inf")
    offtrack_dbc = offtrack_db - main_db
    post_peak_dbc = post_peak_db - main_db

    passed = (
        rejection_db >= args.min_rejection_db
        and offtrack_dbc <= args.max_offtrack_dbc
        and post_peak_dbc <= args.max_post_peak_dbc
    )

    print(
        f"{path}: cutoff={cutoff:.3f}s "
        f"pre={pre_db:.2f} dBFS post={post_db:.2f} dBFS "
        f"rejection={rejection_db:.2f} dB "
        f"offtrack={offtrack_dbc:.2f} dBc post_peak={post_peak_dbc:.2f} dBc "
        f"{'PASS' if passed else 'FAIL'}"
    )

    return passed


def main():
    parser = argparse.ArgumentParser(description="Measure sweep rejection after target Nyquist crossing.")
    parser.add_argument("paths", nargs="+", help="WAV files to analyze")
    parser.add_argument("--target-rate", type=int, required=True, help="output sample rate")
    parser.add_argument("--src-rate", type=int, required=True, help="source sample rate (for sweep end freq)")
    parser.add_argument("--duration", type=float, default=8.0)
    parser.add_argument("--guard", type=float, default=0.25)
    parser.add_argument("--pre-window", type=float, default=1.0)
    parser.add_argument("--min-rejection-db", type=float, default=60.0)
    parser.add_argument("--fft-size", type=int, default=4096)
    parser.add_argument("--track-width-hz", type=float, default=800.0)
    parser.add_argument("--max-offtrack-dbc", type=float, default=-25.0)
    parser.add_argument("--max-post-peak-dbc", type=float, default=-35.0)
    args = parser.parse_args()

    end_freq = args.src_rate * 0.46875
    args.start_freq = 20.0
    args.end_freq = int(end_freq)

    ok = True
    for path_arg in args.paths:
        path = Path(path_arg)
        ok = check_output(path, args.target_rate, args) and ok

    raise SystemExit(0 if ok else 1)


if __name__ == "__main__":
    main()
