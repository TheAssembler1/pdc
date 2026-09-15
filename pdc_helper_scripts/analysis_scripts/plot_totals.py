#!/usr/bin/env python3
"""
Stacked-bar comparison of total workload time across the magnitude-
analysis benchmark modes (eager DataFlyway, lazy DataFlyway, posthoc PDC,
posthoc HDF5), reconstructed from this directory's results_<mode>_*.csv
files. Run extract_close_times.py first (or after) so avg_close_s /
total_with_close_s are populated -- see README.md's "Result CSV schema"
section for why that column matters for an HDF5-comparable total.

Each results CSV has one row per timestep (N_TIMESTEPS=3 in the C
benchmarks), with two kinds of columns: per-timestep costs that really
happen three times (write_s, readback_s, compute_s, writeback_s,
confirm_read_s) and one-time job-level costs that are just repeated on
every row for CSV convenience (setup_s / write_setup_s / analyze_setup_s,
relaunch_s, avg_close_s). Reconstructing one job's true wall-clock "total
workload time" means summing the former across every timestep row and
taking the latter once, not 3x -- that's what aggregate() below does
before stacking. Segments that are always zero for a given mode (e.g.
eager's readback_s/compute_s/writeback_s, folded into write_s instead --
see README.md) are dropped from that mode's stack instead of drawing an
empty slice.

Usage:
    python3 plot_totals.py [--results-dir DIR] [--out FILE.png] [--modes eager,lazy,posthoc,hdf5]

Requires: numpy, matplotlib (no pandas).
"""
import argparse
import csv
import glob
import os
from collections import defaultdict

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

# mode -> (one_time_cols, per_timestep_cols). Must track README.md's
# "Result CSV schema" section and the awk post-processing in
# eager_pdc.sbatch / lazy_pdc.sbatch / posthoc_pdc.sbatch /
# posthoc_hdf5.sbatch.
_EAGER_LAZY_ONE_TIME = ["setup_s"]
_EAGER_LAZY_PER_STEP = ["write_s", "readback_s", "compute_s", "writeback_s", "confirm_read_s"]
_POSTHOC_ONE_TIME = ["write_setup_s", "relaunch_s", "analyze_setup_s"]
_POSTHOC_PER_STEP = ["write_s", "readback_s", "compute_s", "writeback_s"]

SCHEMAS = {
    "eager": (_EAGER_LAZY_ONE_TIME, _EAGER_LAZY_PER_STEP),
    "lazy": (_EAGER_LAZY_ONE_TIME, _EAGER_LAZY_PER_STEP),
    "posthoc": (_POSTHOC_ONE_TIME, _POSTHOC_PER_STEP),
    "hdf5": (_POSTHOC_ONE_TIME, _POSTHOC_PER_STEP),
}

# Fixed stacking order (bottom to top) and one color per cost segment,
# shared across modes so the same cost always means the same color.
SEGMENT_ORDER = [
    "setup_s", "write_setup_s", "analyze_setup_s",
    "write_s", "readback_s", "compute_s", "writeback_s", "confirm_read_s",
    "relaunch_s", "avg_close_s",
]
SEGMENT_COLOR = {
    "setup_s": "#8c8c8c",
    "write_setup_s": "#8c8c8c",
    "analyze_setup_s": "#bfbfbf",
    "write_s": "#4477AA",
    "readback_s": "#EE6677",
    "compute_s": "#228833",
    "writeback_s": "#CCBB44",
    "confirm_read_s": "#66CCEE",
    "relaunch_s": "#AA3377",
    "avg_close_s": "#222222",
}
SEGMENT_LABEL = {
    "setup_s": "setup",
    "write_setup_s": "write setup",
    "analyze_setup_s": "analyze setup",
    "write_s": "write",
    "readback_s": "readback",
    "compute_s": "compute",
    "writeback_s": "writeback",
    "confirm_read_s": "confirm read",
    "relaunch_s": "relaunch",
    "avg_close_s": "server close",
}

MODE_ORDER = ["eager", "lazy", "posthoc", "hdf5"]
MODE_LABEL = {
    "eager": "DataFlyway (eager)",
    "lazy": "DataFlyway (lazy)",
    "posthoc": "PDC (posthoc)",
    "hdf5": "HDF5 (posthoc)",
}
# Same-color bars are told apart by hatch, not just legend position.
MODE_HATCH = {"eager": "", "lazy": "//", "posthoc": "xx", "hdf5": ".."}

FLOAT_BYTES = 4
N_VARS = 3  # vx, vy, vz


def read_results_csv(path):
    """Yield dict rows from one results_<mode>_<jobid>.csv, skipping FAILED
    sentinel rows (short row -- see srun_client_eager.sh etc.)."""
    with open(path, newline="") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        if not header:
            return
        for row in reader:
            if len(row) != len(header):
                continue
            yield dict(zip(header, row))


def load_all(results_dir):
    """mode -> n_ranks -> rows (one job's timestep rows). When more than
    one results_<mode>_<jobid>.csv exists for the same n_ranks (a rerun),
    keep only the most-recently-modified file's rows rather than mixing
    two different jobs' timesteps together."""
    best = {}  # (mode, n_ranks) -> (mtime, rows)
    for mode in MODE_ORDER:
        for path in glob.glob(os.path.join(results_dir, f"results_{mode}_*.csv")):
            mtime = os.path.getmtime(path)
            rows_by_ranks = defaultdict(list)
            for row in read_results_csv(path):
                try:
                    n_ranks = int(row["n_ranks"])
                except (KeyError, ValueError):
                    continue
                rows_by_ranks[n_ranks].append(row)
            for n_ranks, rows in rows_by_ranks.items():
                key = (mode, n_ranks)
                if key not in best or mtime > best[key][0]:
                    best[key] = (mtime, rows)
    data = defaultdict(dict)
    for (mode, n_ranks), (_, rows) in best.items():
        data[mode][n_ranks] = rows
    return data


def aggregate(mode, rows):
    """Reconstruct one job's true total workload time, stacked by segment.

    One-time columns (setup_s, relaunch_s, avg_close_s, ...) are constant
    across a job's timestep rows -- take the mean, which equals that
    constant. Per-timestep columns are summed across every timestep row
    actually present, since each one really happened that many times.

    Returns (segments: dict[col] -> seconds, data_gb).
    """
    one_time_cols, per_step_cols = SCHEMAS[mode]
    segments = {}
    for col in one_time_cols:
        vals = [float(r[col]) for r in rows if r.get(col, "") not in ("", "FAILED")]
        segments[col] = float(np.mean(vals)) if vals else 0.0
    for col in per_step_cols:
        vals = [float(r[col]) for r in rows if r.get(col, "") not in ("", "FAILED")]
        segments[col] = float(np.sum(vals))
    close_vals = [float(r["avg_close_s"]) for r in rows if r.get("avg_close_s", "") not in ("", "FAILED")]
    segments["avg_close_s"] = float(np.mean(close_vals)) if close_vals else 0.0

    n_elem = float(rows[0]["n_elem"]) if rows else 0.0
    n_ranks = float(rows[0]["n_ranks"]) if rows else 0.0
    n_timesteps = len(rows)
    # Aggregate INPUT data size (vx+vy+vz, float32) written across every
    # timestep present for this job -- the x-axis label (n_ranks /
    # data_size_GB) describes the whole workload the stacked total above
    # it represents, not a single timestep.
    data_gb = n_ranks * n_elem * FLOAT_BYTES * N_VARS * n_timesteps / 1e9
    return segments, data_gb


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--results-dir", default=".", help="Directory holding results_<mode>_*.csv (default: .)")
    ap.add_argument("--out", default="totals_comparison.png", help="Output image path")
    ap.add_argument("--modes", default=",".join(MODE_ORDER), help="Comma-separated subset of modes to plot")
    args = ap.parse_args()

    modes = [m.strip() for m in args.modes.split(",") if m.strip()]
    for m in modes:
        if m not in SCHEMAS:
            raise SystemExit(f"Unknown mode '{m}', expected one of {list(SCHEMAS)}")

    data = load_all(args.results_dir)

    per_job = defaultdict(dict)  # mode -> n_ranks -> (segments, data_gb)
    for mode in modes:
        for n_ranks, rows in data[mode].items():
            per_job[mode][n_ranks] = aggregate(mode, rows)

    modes_present = [m for m in modes if per_job[m]]
    if not modes_present:
        raise SystemExit(f"No results_<mode>_*.csv rows found under {args.results_dir!r} for modes {modes}")

    all_ranks = sorted({n for m in modes_present for n in per_job[m]})

    def label_for(n_ranks):
        # Prefer whichever mode has data at this rank count, in
        # MODE_ORDER, for the paired data_gb figure -- all modes should
        # agree on data_gb at the same n_ranks (same N_ELEM), so which one
        # wins here is cosmetic.
        for m in modes_present:
            if n_ranks in per_job[m]:
                _, gb = per_job[m][n_ranks]
                return f"{n_ranks}/{gb:.2f}GB"
        return str(n_ranks)

    n_groups = len(all_ranks)
    n_bars = len(modes_present)
    group_width = 0.8
    bar_width = group_width / max(n_bars, 1)

    fig, ax = plt.subplots(figsize=(max(7.0, 1.7 * n_groups), 5.5))
    x = np.arange(n_groups)

    legend_segment_handles = {}
    for bi, mode in enumerate(modes_present):
        offset = (bi - (n_bars - 1) / 2) * bar_width
        bottoms = np.zeros(n_groups)
        for seg in SEGMENT_ORDER:
            heights = np.array(
                [per_job[mode].get(n_ranks, (dict(), 0.0))[0].get(seg, 0.0) for n_ranks in all_ranks]
            )
            if not np.any(heights > 0):
                continue  # this mode never has a nonzero value for this segment
            bars = ax.bar(
                x + offset, heights, bar_width * 0.92, bottom=bottoms,
                color=SEGMENT_COLOR[seg], hatch=MODE_HATCH[mode],
                edgecolor="white", linewidth=0.4, zorder=3,
            )
            bottoms += heights
            legend_segment_handles.setdefault(seg, bars[0])

    ax.set_xticks(x)
    ax.set_xticklabels([label_for(n) for n in all_ranks], rotation=0)
    ax.set_xlabel("ranks / data size (GB)")
    ax.set_ylabel("total workload time (s)")
    ax.set_title("HDF5 vs. DataFlyway (PDC) total workload time")
    ax.yaxis.grid(True, linestyle="--", alpha=0.4, zorder=0)
    ax.set_axisbelow(True)

    seg_handles = [legend_segment_handles[s] for s in SEGMENT_ORDER if s in legend_segment_handles]
    seg_labels = [SEGMENT_LABEL[s] for s in SEGMENT_ORDER if s in legend_segment_handles]
    leg1 = ax.legend(seg_handles, seg_labels, title="cost segment", loc="upper left", fontsize=8, title_fontsize=8)
    ax.add_artist(leg1)

    mode_handles = [
        plt.Rectangle((0, 0), 1, 1, facecolor="white", edgecolor="black", hatch=MODE_HATCH[m])
        for m in modes_present
    ]
    mode_labels = [MODE_LABEL[m] for m in modes_present]
    ax.legend(mode_handles, mode_labels, title="workload", loc="upper right", fontsize=8, title_fontsize=8)

    fig.tight_layout()
    fig.savefig(args.out, dpi=200)
    print(f"Wrote {args.out}")


if __name__ == "__main__":
    main()
