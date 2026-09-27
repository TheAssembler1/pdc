#!/usr/bin/env python3
"""
Curl + vorticity-magnitude workload comparison: for each rank count, one
stacked bar per PDC server count (2/4/8/... eager/DataFlyway runs,
auto-discovered) with a linear best-fit line through those totals to show
strong scaling, plus one additional reference bar per other method that
has real result data -- PDC posthoc, HDF5 posthoc, ADIOS2 -- placed in
the same rank-count group so all methods are visually comparable.

Data layout this expects (see ../README.md and the sibling
plot_curl_totals.py, which this script borrows its aggregation logic and
color/legend conventions from):

    curl_analysis_scripts/
      2_servers_<date>/results_curl_eager_<jobid>.csv
      4_servers_<date>/results_curl_eager_<jobid>.csv
      8_servers_<date>/results_curl_eager_<jobid>.csv, results_curl_posthoc_<jobid>.csv, ...
      ...                                         <- more <N>_servers* dirs will show up
      csv_res/, and loose results_curl_<mode>_<jobid>.csv files anywhere
        else under the root                       <- also scanned for non-eager modes
      plot_res_pngs/plot_curl_comparison.py        <- this file

Eager's server count is parsed from the leading "<N>_servers" of each
directory name directly under --results-root -- new "<N>_servers*"
directories that get pushed later are picked up automatically, no code
change needed, and each becomes its own bar plus a point on the
strong-scaling trend line. The other modes (posthoc/hdf5/adios2_curl/
eager_compress) don't have a comparable per-server-count sweep in the
data on disk today, so each gets exactly one bar per rank count,
searched recursively under --results-root by filename
(results_curl_<mode>_*.csv) rather than by directory -- wherever that
mode's CSVs happen to live.

As of this script's writing, real result data only exists for eager (at
2/4/8 servers) and posthoc (at 8 servers) -- results_curl_hdf5_*.csv,
results_curl_adios2_*.csv, and results_curl_eager_compress_*.csv (the
one eager_compress file on disk is header-only, zero data rows) don't
exist yet anywhere in the repo. Those three are still fully wired up
(schema, color, label) so they appear automatically, no code change
required, the moment those jobs are actually run and their CSVs land.

Every results_curl_<mode>_<jobid>.csv has one row per timestep, with
one-time job costs (setup_s, write_setup_s, relaunch_s, analyze_setup_s,
avg_close_s) repeated on every row (take the mean) and per-timestep
costs (write_s, readback_s, curl_compute_s, curl_writeback_s,
magnitude_compute_s, magnitude_writeback_s, adios2_curl's generic
compute_s/writeback_s) summed across every timestep row actually
present -- same reconstruction as plot_curl_totals.py's aggregate().
confirm_read_s is deliberately excluded everywhere: it's each
benchmark's own post-write correctness check (see bench_curl_eager.c /
adios2_bench_curl.cpp), not workload cost being measured, the identical
reasoning plot_curl_totals.py already applies.

Raw columns fold into four coarse segments shared across every mode
(RAW_TO_SEGMENT), the same "sim write / analysis read / analysis
compute / analysis write" split plot_totals.py (the Drive-hosted
magnitude-analysis version of this script) uses for its own six-segment
version of this idea. Eager has no separate analysis phase at all (the
server computes curl+magnitude synchronously inside write_s), so its
bars only ever show the sim_write segment -- expected, not a bug.

Usage:
    python3 plot_curl_comparison.py [--results-root DIR] [--out FILE.png]

Rerun any time new "<N>_servers*" directories or results_curl_<mode>_*.csv
files land under --results-root.

Requires: numpy, matplotlib (no pandas).
"""
import argparse
import csv
import glob
import os
import re
from collections import defaultdict

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import AutoMinorLocator
import matplotlib.patches as mpatches
import matplotlib.transforms as mtransforms

# mode -> (one_time_cols, per_timestep_cols). Must track README.md's
# "Result CSV schemas" section. confirm_read_s intentionally omitted
# everywhere -- see module docstring.
_EAGER_ONE_TIME = ["setup_s", "avg_close_s"]
_EAGER_PER_STEP = ["write_s"]
_POSTHOC_ONE_TIME = ["write_setup_s", "relaunch_s", "analyze_setup_s", "avg_close_s"]
_POSTHOC_PER_STEP = [
    "write_s", "readback_s", "curl_compute_s", "curl_writeback_s",
    "magnitude_compute_s", "magnitude_writeback_s",
]
_HDF5_ONE_TIME = ["write_setup_s", "analyze_setup_s"]
_HDF5_PER_STEP = _POSTHOC_PER_STEP
# adios2_curl (adios2_bench_curl.cpp) has no server, so no avg_close_s,
# and its client-side fallback compute (see that file's header comment)
# only produces one generic compute_s/writeback_s pair, not a separate
# curl_*/magnitude_* split.
_ADIOS2_CURL_ONE_TIME = ["setup_s"]
_ADIOS2_CURL_PER_STEP = ["write_s", "readback_s", "compute_s", "writeback_s"]

# mode -> filename glob pattern to search for recursively under
# --results-root (anchored per-file below so a shorter mode name can't
# glob-match a longer mode's files, e.g. "eager" vs "eager_compress").
MODE_SCHEMAS = {
    "eager_compress": (_EAGER_ONE_TIME, _EAGER_PER_STEP),
    "posthoc": (_POSTHOC_ONE_TIME, _POSTHOC_PER_STEP),
    "hdf5": (_HDF5_ONE_TIME, _HDF5_PER_STEP),
    "adios2_curl": (_ADIOS2_CURL_ONE_TIME, _ADIOS2_CURL_PER_STEP),
}
# "eager" itself is handled separately (per-server-count directory
# discovery, see discover_server_dirs), not through MODE_SCHEMAS/glob.
EAGER_ONE_TIME, EAGER_PER_STEP = _EAGER_ONE_TIME, _EAGER_PER_STEP

# Raw CSV column -> one of five coarse cost segments, shared across every
# mode so the same color always means the same phase. Eager/eager_compress
# only ever populate sim_write + close; posthoc/hdf5/adios2_curl populate
# all five. avg_close_s (PDC's close_server checkpoint/flush) is its own
# segment, separate from sim_write, per explicit request -- it's a real,
# distinct cost (see analysis_scripts/README.md's own rationale for why
# it's measured at all: the region cache can hide a write's true durable-
# persist cost otherwise), not a footnote folded into "write".
RAW_TO_SEGMENT = {
    "setup_s": "sim_write",
    "write_setup_s": "sim_write",
    "write_s": "sim_write",
    "relaunch_s": "sim_write",
    "avg_close_s": "close",
    "readback_s": "ana_read",
    "analyze_setup_s": "ana_compute",
    "curl_compute_s": "ana_compute",
    "magnitude_compute_s": "ana_compute",
    "compute_s": "ana_compute",
    "curl_writeback_s": "ana_write",
    "magnitude_writeback_s": "ana_write",
    "writeback_s": "ana_write",
}
SEGMENT_ORDER = ["sim_write", "close", "ana_read", "ana_compute", "ana_write"]
SEGMENT_LABEL = {
    "sim_write": "write (setup + write + relaunch)",
    "close": "server close",
    "ana_read": "analysis read",
    "ana_compute": "analysis compute",
    "ana_write": "analysis write",
}
# Validated categorical quintuple (dataviz reference palette, slots 1-5),
# passing lightness/chroma/CVD/normal-vision checks on both light and dark
# surfaces -- see references/palette.md. Hatch texture doubles as the
# secondary encoding the aqua/yellow/magenta slots' light-mode contrast
# WARN calls for.
SEGMENT_COLOR = {
    "sim_write": "#2a78d6", "close": "#e87ba4", "ana_read": "#eb6834",
    "ana_compute": "#1baf7a", "ana_write": "#eda100",
}
SEGMENT_HATCH = {"sim_write": "..", "close": "||", "ana_read": "//", "ana_compute": "xx", "ana_write": "\\\\"}

# Per-server-count identity color for eager's bars (label text only --
# fill color is reserved for cost-segment identity above), extended with
# more entries so future server counts still get a distinct color.
SERVER_COLOR_CYCLE = ["#111111", "#7D3C98", "#B03A2E", "#0E7C61", "#1A5276", "#7E5109"]

# Other-method identity: (short bar-label text, label color). Deliberately
# a different hue family than SERVER_COLOR_CYCLE's dark neutrals (its
# first 3 entries are already in active use by eager's 2/4/8-server bars
# today) so a method label never accidentally reads as "matches some
# server-count bar" -- see the strong_scaling bug this replaced, where
# posthoc and "4 servers" both landed on #7D3C98.
OTHER_MODE_ORDER = ["eager_compress", "posthoc", "hdf5", "adios2_curl"]
OTHER_MODE_SHORT = {"eager_compress": "ZC", "posthoc": "PH", "hdf5": "H5", "adios2_curl": "A2"}
OTHER_MODE_COLOR = {"eager_compress": "#D81B60", "posthoc": "#00897B", "hdf5": "#F9A825", "adios2_curl": "#5C6BC0"}
OTHER_MODE_LABEL = {
    "eager_compress": "eager, GPU-ZFP compressed",
    "posthoc": "PDC posthoc (8 servers)",
    "hdf5": "HDF5 posthoc",
    "adios2_curl": "ADIOS2 (client-side compute)",
}
# Short form for the rotated per-bar label (kept under ~13 characters, same
# length budget as plot_magnitude_comparison.py's MODE_LABEL strings, so
# the rotated text never grows tall enough to collide with the ranks/GB
# tick labels below it) -- the full description above still appears in
# the "method / series" legend.
OTHER_MODE_BAR_LABEL = {"eager_compress": "Eager+ZFP", "posthoc": "PDC Posthoc", "hdf5": "HDF5 Posthoc", "adios2_curl": "ADIOS2"}

TREND_COLOR = "#52514e"

SERVER_DIR_RE = re.compile(r"^(\d+)_servers")

# Data-size label on the x-axis: u/v/w (float32) plus curl_x/y/z and
# vorticity_magnitude (float64), all persisted once per timestep -- same
# formula plot_curl_totals.py's aggregate() uses for its own x-axis label.
FLOAT_BYTES = 4
DOUBLE_BYTES = 8
N_INPUT_VARS = 3  # u, v, w
N_CURL_VARS = 3  # curl_x, curl_y, curl_z
N_MAG_VARS = 1  # vorticity_magnitude
BYTES_PER_ELEM = FLOAT_BYTES * N_INPUT_VARS + DOUBLE_BYTES * (N_CURL_VARS + N_MAG_VARS)


def read_results_csv(path):
    with open(path, newline="") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        if not header:
            return
        for row in reader:
            if len(row) != len(header):
                continue
            yield dict(zip(header, row))


def _rows_by_ranks_from_files(paths):
    """n_ranks -> rows, keeping only the newest jobid's rows per n_ranks
    (a rerun) rather than mixing two jobs' timesteps together."""
    best = {}  # n_ranks -> (jobid, rows)
    jobid_re = re.compile(r"_(\d+)\.csv$")
    for path in paths:
        m = jobid_re.search(os.path.basename(path))
        if not m:
            continue
        jobid = int(m.group(1))
        rows_by_ranks = defaultdict(list)
        for row in read_results_csv(path):
            try:
                n_ranks = int(row["n_ranks"])
            except (KeyError, ValueError):
                continue
            rows_by_ranks[n_ranks].append(row)
        for n_ranks, rows in rows_by_ranks.items():
            if n_ranks not in best or jobid > best[n_ranks][0]:
                best[n_ranks] = (jobid, rows)
    return {n_ranks: rows for n_ranks, (_, rows) in best.items()}


def discover_server_dirs(results_root):
    """server_count (int) -> directory path, for every <N>_servers* dir
    directly under results_root."""
    found = {}
    for entry in sorted(os.listdir(results_root)):
        full = os.path.join(results_root, entry)
        if not os.path.isdir(full):
            continue
        m = SERVER_DIR_RE.match(entry)
        if not m:
            continue
        n_servers = int(m.group(1))
        if n_servers in found:
            print(f"NOTE: multiple dirs match {n_servers}_servers* ({found[n_servers]!r}, {full!r}); using {full!r}")
        found[n_servers] = full
    return found


def load_eager_dir(dir_path):
    return _rows_by_ranks_from_files(glob.glob(os.path.join(dir_path, "results_curl_eager_*.csv")))


def load_mode_anywhere(root, mode, filename_prefix="results_curl_"):
    """n_ranks -> rows, searching recursively under root for
    <filename_prefix><mode>_<jobid>.csv (anchored so e.g. "eager" doesn't
    glob-match "eager_compress"). adios2_curl's real files
    (adios2_analysis_test/results_adios2_curl_<jobid>.csv) follow
    adios2_analysis_test's own "results_adios2_<workload>" naming, not
    this directory's "results_curl_<mode>" convention -- pass
    filename_prefix="results_" for that one, see main()."""
    name_re = re.compile(rf"^{re.escape(filename_prefix)}{re.escape(mode)}_\d+\.csv$")
    paths = [
        p for p in glob.glob(os.path.join(root, "**", f"{filename_prefix}{mode}_*.csv"), recursive=True)
        if name_re.match(os.path.basename(p))
    ]
    return _rows_by_ranks_from_files(paths)


def aggregate(rows, one_time_cols, per_step_cols):
    """One job's true workload total, folded into the four coarse
    segments -- one-time columns meaned, per-step columns summed across
    every timestep row actually present. Returns (segments, data_gb)."""
    raw = {}
    for col in one_time_cols:
        vals = [float(r[col]) for r in rows if r.get(col, "") not in ("", "FAILED")]
        raw[col] = float(np.mean(vals)) if vals else 0.0
    for col in per_step_cols:
        vals = [float(r[col]) for r in rows if r.get(col, "") not in ("", "FAILED")]
        raw[col] = float(np.sum(vals))
    folded = {seg: 0.0 for seg in SEGMENT_ORDER}
    for col, val in raw.items():
        folded[RAW_TO_SEGMENT[col]] += val

    nx = float(rows[0]["nx"]) if rows and "nx" in rows[0] else 0.0
    ny = float(rows[0]["ny"]) if rows and "ny" in rows[0] else 0.0
    nz_per_rank = float(rows[0]["nz_per_rank"]) if rows and "nz_per_rank" in rows[0] else 0.0
    n_ranks = float(rows[0]["n_ranks"]) if rows else 0.0
    n_timesteps = len(rows)
    data_gb = n_ranks * (nx * ny * nz_per_rank) * BYTES_PER_ELEM * n_timesteps / 1e9
    return folded, data_gb


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument(
        "--results-root", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."),
        help="Directory holding <N>_servers* subdirectories and/or results_curl_<mode>_*.csv files (default: parent of this script's directory)",
    )
    ap.add_argument(
        "--adios2-root", default=None,
        help="Directory holding results_adios2_curl_*.csv (default: <results-root>/../../adios2_analysis_test)",
    )
    ap.add_argument("--out", default=None, help="Output PNG path (default: <script dir>/curl_comparison.png)")
    args = ap.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    out_path = args.out or os.path.join(script_dir, "curl_comparison.png")
    adios2_root = args.adios2_root or os.path.normpath(
        os.path.join(args.results_root, "..", "..", "adios2_analysis_test")
    )

    server_dirs = discover_server_dirs(args.results_root)
    if not server_dirs:
        raise SystemExit(f"No <N>_servers* directories found under {args.results_root!r}")
    server_counts = sorted(server_dirs)
    print(f"Found eager server counts: {server_counts}")

    # bar series, in plotting order: [("eager", 2), ("eager", 4), ("eager", 8), ("posthoc", None), ...]
    series = [("eager", n) for n in server_counts]

    # series key -> n_ranks -> segments dict
    per_series = {}
    for n_servers in server_counts:
        rows_by_ranks = load_eager_dir(server_dirs[n_servers])
        per_series[("eager", n_servers)] = {
            n_ranks: aggregate(rows, EAGER_ONE_TIME, EAGER_PER_STEP) for n_ranks, rows in rows_by_ranks.items()
        }

    for mode in OTHER_MODE_ORDER:
        one_time_cols, per_step_cols = MODE_SCHEMAS[mode]
        # adios2_curl's real files live under adios2_analysis_test/ as
        # results_adios2_curl_<jobid>.csv (that directory's own naming
        # convention), not this directory's results_curl_<mode> pattern.
        if mode == "adios2_curl":
            root, prefix = adios2_root, "results_"
        else:
            root, prefix = args.results_root, "results_curl_"
        rows_by_ranks = load_mode_anywhere(root, mode, filename_prefix=prefix)
        segs_by_ranks = {n_ranks: aggregate(rows, one_time_cols, per_step_cols) for n_ranks, rows in rows_by_ranks.items()}
        if not segs_by_ranks:
            print(f"NOTE: no {prefix}{mode}_*.csv data found under {root!r} -- skipping")
            continue
        per_series[(mode, None)] = segs_by_ranks
        series.append((mode, None))

    all_ranks = sorted({n for segs in per_series.values() for n in segs})
    if not all_ranks:
        raise SystemExit("No results_curl_*.csv rows found anywhere under the results root")

    server_color = {n: SERVER_COLOR_CYCLE[i % len(SERVER_COLOR_CYCLE)] for i, n in enumerate(server_counts)}

    def bar_label(key):
        mode, n_servers = key
        return f"{n_servers} servers" if mode == "eager" else OTHER_MODE_BAR_LABEL[mode]

    def bar_label_color(key):
        mode, n_servers = key
        return server_color[n_servers] if mode == "eager" else OTHER_MODE_COLOR[mode]

    n_groups = len(all_ranks)
    n_bars = len(series)
    group_width = 0.74
    slot_width = group_width / max(n_bars, 1)
    bar_width = slot_width * 0.8
    x = np.arange(n_groups)

    fig, ax = plt.subplots(figsize=(max(9.0, 1.9 * n_groups), 8.0), constrained_layout=True)
    fig.set_facecolor("#fcfcfb")
    ax.set_facecolor("#fcfcfb")

    bar_centers = {}  # (key, group_index) -> x position
    for bi, key in enumerate(series):
        offset = (bi - (n_bars - 1) / 2) * slot_width
        bottoms = np.zeros(n_groups)
        for seg in SEGMENT_ORDER:
            heights = np.array([per_series[key].get(n_ranks, ({}, 0.0))[0].get(seg, 0.0) for n_ranks in all_ranks])
            if not np.any(heights > 0):
                continue
            ax.bar(
                x + offset, heights, bar_width, bottom=bottoms,
                color=SEGMENT_COLOR[seg], hatch=SEGMENT_HATCH[seg],
                edgecolor="white", linewidth=0.6, zorder=3,
            )
            bottoms += heights
        for gi, n_ranks in enumerate(all_ranks):
            bar_centers[(key, gi)] = x[gi] + offset

    # Workload-type label underneath each bar, rotated and colored by
    # series identity -- same convention plot_magnitude_comparison.py
    # uses, per explicit request.
    label_trans = mtransforms.blended_transform_factory(ax.transData, ax.transAxes)
    for key in series:
        for gi, n_ranks in enumerate(all_ranks):
            if n_ranks not in per_series[key]:
                continue
            ax.text(
                bar_centers[(key, gi)], -0.02, bar_label(key),
                transform=label_trans, rotation=90, ha="center", va="top",
                fontsize=8.5, fontweight="bold", color=bar_label_color(key), clip_on=False,
            )
    # Smaller, darker ranks/GB tick labels so they read clearly against
    # the taller rotated bar labels crowding the space right above them.
    ax.tick_params(axis="x", pad=88, labelsize=8, labelcolor="#111111")

    # A line spanning each rank-count group's own bars, sitting just above
    # that group's ranks/GB tick label, so it's visually obvious which
    # bars belong to which label -- per explicit request.
    group_line_trans = mtransforms.blended_transform_factory(ax.transData, ax.transAxes)
    for gi, n_ranks in enumerate(all_ranks):
        group_keys = [k for k in series if n_ranks in per_series[k]]
        if not group_keys:
            continue
        xs = [bar_centers[(k, gi)] for k in group_keys]
        ax.plot(
            [min(xs) - bar_width / 2, max(xs) + bar_width / 2], [-0.195, -0.195],
            transform=group_line_trans, color="#999999", linewidth=1.0, clip_on=False, zorder=6,
        )

    # Linear best-fit line through each rank-size group's per-server-count
    # eager totals only (posthoc/hdf5/adios2_curl are different methods,
    # not a "more servers" scaling knob, so a fit across them wouldn't mean
    # strong scaling) -- fit against bar x-position so it renders straight
    # regardless of server counts not being evenly spaced in real units.
    trend_label_used = False
    for gi, n_ranks in enumerate(all_ranks):
        xs, ys = [], []
        for n_servers in server_counts:
            key = ("eager", n_servers)
            if n_ranks in per_series[key]:
                xs.append(bar_centers[(key, gi)])
                ys.append(sum(per_series[key][n_ranks][0].values()))
        if len(xs) < 2:
            continue
        coeffs = np.polyfit(xs, ys, 1)
        fit_xs = np.linspace(min(xs), max(xs), 20)
        fit_ys = np.polyval(coeffs, fit_xs)
        ax.plot(
            fit_xs, fit_ys, linestyle="--", linewidth=1.6, color=TREND_COLOR, zorder=4, alpha=0.85,
            label="linear fit (eager strong scaling)" if not trend_label_used else None,
        )
        trend_label_used = True

    def rank_gb_label(n_ranks):
        # Prefer whichever series has data at this rank count for the
        # paired data-size figure -- every series should agree on data_gb
        # at the same n_ranks (same NX/NY/NZ_PER_RANK), so which one wins
        # here is cosmetic. Same convention as plot_curl_totals.py.
        for k in series:
            if n_ranks in per_series[k]:
                _, gb = per_series[k][n_ranks]
                return f"{n_ranks}\n{gb:.2f}GB"
        return str(n_ranks)

    ax.set_xticks(x)
    ax.set_xticklabels([rank_gb_label(n) for n in all_ranks])
    ax.set_xlabel("MPI ranks / data size (GB)")
    ax.set_ylabel("total workload time (s)")
    ax.set_title("curl + vorticity-magnitude: eager strong scaling vs. other methods")
    ax.yaxis.set_minor_locator(AutoMinorLocator(2))
    ax.yaxis.grid(True, which="major", linestyle="-", linewidth=0.8, color="#888888", alpha=0.7, zorder=0)
    ax.yaxis.grid(True, which="minor", linestyle="-", linewidth=0.5, color="#aaaaaa", alpha=0.5, zorder=0)
    ax.set_axisbelow(True)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    seg_keys = [
        s for s in SEGMENT_ORDER
        if any(per_series[k].get(n, ({}, 0.0))[0].get(s, 0.0) > 0 for k in series for n in all_ranks)
    ]
    seg_handles = [mpatches.Patch(facecolor=SEGMENT_COLOR[s], hatch=SEGMENT_HATCH[s], edgecolor="white") for s in seg_keys]
    seg_labels = [SEGMENT_LABEL[s] for s in seg_keys]
    leg1 = ax.legend(seg_handles, seg_labels, title="cost segment", loc="upper left", fontsize=8, title_fontsize=8)
    ax.add_artist(leg1)

    series_handles = [
        plt.Line2D([0], [0], marker="s", linestyle="none", markersize=8,
                   markerfacecolor=bar_label_color(k), markeredgecolor=bar_label_color(k))
        for k in series
    ]
    series_labels = [f"eager, {n} servers" if mode == "eager" else OTHER_MODE_LABEL[mode] for mode, n in series]
    trend_handle = plt.Line2D([0], [0], linestyle="--", linewidth=1.6, color=TREND_COLOR)
    ax.legend(
        series_handles + [trend_handle], series_labels + ["linear fit (eager, per rank count)"],
        title="method / series", loc="upper right", fontsize=8, title_fontsize=8,
    )

    y_max = max((sum(segs.values()) for k in series for segs, _ in per_series[k].values()), default=1.0)
    ax.set_ylim(0, y_max * 1.18)

    fig.savefig(out_path, dpi=200)
    print(f"Wrote {out_path}")


if __name__ == "__main__":
    main()
