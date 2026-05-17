#!/usr/bin/env python3
import argparse
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from scipy.io import wavfile
from scipy.signal import spectrogram, windows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--title", default="")
    parser.add_argument("--width", type=int, default=4800)
    args = parser.parse_args()

    sr, x = wavfile.read(str(args.input))
    if x.ndim > 1:
        x = x[:, 0]
    if np.issubdtype(x.dtype, np.integer):
        x = x.astype(np.float64) / np.iinfo(x.dtype).max
    else:
        x = x.astype(np.float64)

    win = windows.chebwin(2048, at=100)
    f, t, Sxx = spectrogram(x, fs=sr, window=win, nperseg=2048, noverlap=1920,
                            scaling="density", mode="magnitude")
    Sxx_db = 20.0 * np.log10(Sxx + 1e-300)

    dpi = 150
    width_in = args.width / dpi
    height_in = width_in * 0.66

    fig, ax = plt.subplots(figsize=(width_in, height_in), dpi=dpi)
    extent = [t[0], t[-1], 0, sr / 2]
    ax.imshow(Sxx_db, aspect="auto", origin="lower", extent=extent,
              cmap="inferno", vmin=-100, vmax=0, interpolation="bilinear")
    ax.set_ylim(0, sr / 2)

    ax.set_xlabel("Time (s)", fontsize=56)
    ax.set_ylabel("Frequency (Hz)", fontsize=56)
    ax.tick_params(labelsize=48)

    nyquist = sr // 2
    yticks = [0, nyquist // 4, nyquist // 2, 3 * nyquist // 4, nyquist]
    ylabels = [f"{h // 1000}k" if h >= 1000 else "0" for h in yticks]
    ax.set_yticks(yticks[:len(ylabels)])
    ax.set_yticklabels(ylabels[:len(yticks)])

    if args.title:
        ax.set_title(args.title, fontsize=64)

    fig.subplots_adjust(left=0.12, right=0.98, top=0.91, bottom=0.10)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(str(args.output), dpi=dpi)
    plt.close(fig)


if __name__ == "__main__":
    main()
