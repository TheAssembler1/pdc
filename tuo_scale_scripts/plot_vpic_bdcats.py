#!/usr/bin/env python3
"""
Sync vs async comparison plots for vpic_bdcats scaling-sweep results
(run_sync.sh / run_async.sh's vpic_bdcats_<mode>_<nodes>.csv files, one
per node count -- see tuo_scale_scripts/README.md's CSV schema section).

Reads every vpic_bdcats_<mode>_<nodes>.csv found under the given
directories, using each file's own trailing metadata comment line
(`# n_nodes=...,servers_per_node=...,clients_per_node=...,mode=...,
steps=...,numparticles_per_rank=...,async_sleep_s=...`) rather than
parsing the filename or hardcoding this benchmark's byte-per-particle
constant -- ranks = clients_per_node * n_nodes, and data volume comes
straight from the CSV's own total_data_size_bytes row.

Produces three figures:
  1. Stacked bar: observed I/O time (write below, read above) per mode
     per node count -- the same write/read split vpic_bdcats.c itself
     times (PDCregion_transfer_start_all_mpi through
     PDCregion_transfer_wait_all, whole step including metadata ops,
     minus sleep(sleeptime) for async -- see vpic_bdcats.c's docstring).
  2. Line: total observed I/O time (write+read) vs node count, one line
     per mode.
  3. Line: aggregate throughput (total bytes moved / total observed time,
     not a mean of per-step rates) vs node count, one line per
     mode x direction (write/read).

Async's sleep(sleeptime) between transfer start and wait is standing in
for compute overlapped with in-flight I/O and is already excluded from
every time/throughput value in the CSVs themselves (vpic_bdcats.c), so
sync and async are directly comparable here as real I/O cost, not
wall-clock time.

Usage:
    python3 plot_vpic_bdcats.py results_sync_XXXX results_async_XXXX -o plots/

Requires: numpy, matplotlib (no pandas) -- matches
pdc_helper_scripts/*/plot_*totals.py's convention.
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

MODE_ORDER = ["sync", "async"]
# write = lighter tint, read = base color, same hue per mode -- mirrors
# the write/read-vs-mode encoding used in the HTML sweep report (blue =
# sync, orange = async; light = write, saturated = read).
COLOR = {
    ("sync", "write"): "#86b6ef",
    ("sync", "read"): "#2a78d6",
    ("async", "write"): "#f4ac8f",
    ("async", "read"): "#eb6834",
}
MODE_LINE_COLOR = {"sync": "#2a78d6", "async": "#eb6834"}
MODE_LABEL = {"sync": "Sync", "async": "Async"}

_FILE_RE = re.compile(r"^vpic_bdcats_(sync|async)_(\d+)\.csv$")


def _fmt_count(n):
    """Compact human count: 214748352 -> '215M', 6871947264 -> '6.87B'."""
    n = float(n)
    for div, suffix in ((1e9, "B"), (1e6, "M"), (1e3, "K")):
        if n >= div:
            return f"{n / div:.3g}{suffix}"
    return f"{n:.0f}"


def parse_csv(path):
    """Return (rows, meta) -- rows is a list of dicts from the CSV body
    (record_type,name,step,mean_s,stdev_s,count,value), meta is the
    trailing `# key=value,...` comment line parsed into a dict."""
    meta = {}
    body_lines = []
    with open(path, newline="") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            if line.startswith("#"):
                for kv in line.lstrip("#").strip().split(","):
                    if "=" in kv:
                        k, v = kv.split("=", 1)
                        meta[k.strip()] = v.strip()
            else:
                body_lines.append(line)
    rows = list(csv.DictReader(body_lines))
    return rows, meta


def load_all(dirs):
    """(mode, n_nodes) -> parsed run dict. When more than one
    vpic_bdcats_<mode>_<nodes>.csv exists for the same (mode, n_nodes) (a
    rerun), keep only the most-recently-modified file, matching
    plot_curl_totals.py's convention."""
    best_mtime = {}
    best_path = {}
    for d in dirs:
        for path in glob.glob(os.path.join(d, "vpic_bdcats_*.csv")):
            m = _FILE_RE.match(os.path.basename(path))
            if not m:
                continue
            key = (m.group(1), int(m.group(2)))
            mtime = os.path.getmtime(path)
            if key not in best_mtime or mtime > best_mtime[key]:
                best_mtime[key] = mtime
                best_path[key] = path

    runs = {}
    for key, path in best_path.items():
        rows, meta = parse_csv(path)
        mode = meta.get("mode")
        n_nodes = int(meta.get("n_nodes"))
        clients_per_node = int(meta.get("clients_per_node"))
        steps = int(meta.get("steps"))
        numparticles = int(meta.get("numparticles_per_rank"))
        ranks = clients_per_node * n_nodes
        particles_total = ranks * numparticles

        total_bytes = None
        for r in rows:
            if r["record_type"] == "total_data_size_bytes":
                total_bytes = float(r["value"])
                break
        if total_bytes is None:
            raise SystemExit(f"{path}: missing total_data_size_bytes row")
        bytes_per_step = total_bytes / steps
        gib = total_bytes / 1024**3

        per_step = {"write": {}, "read": {}}
        for r in rows:
            if r["record_type"] == "throughput_write_MBps":
                per_step["write"][int(r["step"])] = float(r["value"])
            elif r["record_type"] == "throughput_read_MBps":
                per_step["read"][int(r["step"])] = float(r["value"])

        time_s = {}
        agg_throughput_mbps = {}
        for direction in ("write", "read"):
            steps_seen = sorted(per_step[direction])
            total_time = sum((bytes_per_step / 1e6) / per_step[direction][s] for s in steps_seen)
            time_s[direction] = total_time
            # Aggregate throughput = total bytes moved / total observed
            # time, not a mean of per-step rates -- correct for
            # unevenly-timed steps, and what "total data / total time"
            # actually means.
            agg_throughput_mbps[direction] = (bytes_per_step * len(steps_seen) / 1e6) / total_time

        runs[key] = dict(
            mode=mode, n_nodes=n_nodes, ranks=ranks, particles_total=particles_total,
            gib=gib, steps=steps, time_s=time_s, throughput_mbps=agg_throughput_mbps,
        )
    return runs


def xtick_label(run):
    return (
        f"{run['n_nodes']} node{'s' if run['n_nodes'] != 1 else ''}\n"
        f"{run['ranks']} ranks\n"
        f"{_fmt_count(run['particles_total'])} particles\n"
        f"{run['gib']:.0f} GiB"
    )


def plot_stacked_bar(runs, all_nodes, out_path):
    n_groups = len(all_nodes)
    group_width = 0.7
    bar_w = group_width / 2 * 0.85
    x = np.arange(n_groups)

    fig, ax = plt.subplots(figsize=(max(7.0, 1.6 * n_groups), 6.0), constrained_layout=True)

    for mi, mode in enumerate(MODE_ORDER):
        offset = (mi - 0.5) * (group_width / 2)
        bottoms = np.zeros(n_groups)
        for direction in ("write", "read"):
            heights = np.array(
                [runs[(mode, n)]["time_s"][direction] if (mode, n) in runs else 0.0 for n in all_nodes]
            )
            ax.bar(
                x + offset, heights, bar_w, bottom=bottoms,
                color=COLOR[(mode, direction)], edgecolor="white", linewidth=0.6,
                label=f"{MODE_LABEL[mode]} {direction}" if n_groups else None,
                zorder=3,
            )
            bottoms += heights
        for xi, n in zip(x, all_nodes):
            if (mode, n) in runs:
                total = sum(runs[(mode, n)]["time_s"].values())
                ax.text(xi + offset, total + 0.02 * bottoms.max(), f"{total:.1f}s",
                        ha="center", va="bottom", fontsize=8)

    ax.set_xticks(x)
    ax.set_xticklabels([xtick_label(runs[(MODE_ORDER[0], n)] if (MODE_ORDER[0], n) in runs
                                     else runs[(MODE_ORDER[1], n)]) for n in all_nodes], fontsize=8)
    ax.set_ylabel("Observed I/O time (s)")
    ax.set_title("vpic_bdcats: sync vs async observed I/O time (write + read, stacked)")
    ax.yaxis.grid(True, linestyle="--", alpha=0.4, zorder=0)
    ax.set_axisbelow(True)

    handles = [
        plt.Rectangle((0, 0), 1, 1, facecolor=COLOR[(m, d)])
        for m in MODE_ORDER for d in ("write", "read")
    ]
    labels = [f"{MODE_LABEL[m]} {d}" for m in MODE_ORDER for d in ("write", "read")]
    ax.legend(handles, labels, loc="upper left", fontsize=8, ncol=2)

    fig.savefig(out_path, dpi=200)
    plt.close(fig)
    print(f"Wrote {out_path}")


def plot_total_time_line(runs, all_nodes, out_path):
    n_groups = len(all_nodes)
    x = np.arange(n_groups)
    fig, ax = plt.subplots(figsize=(max(6.5, 1.4 * n_groups), 5.5), constrained_layout=True)

    for mode in MODE_ORDER:
        totals = np.array(
            [sum(runs[(mode, n)]["time_s"].values()) if (mode, n) in runs else np.nan for n in all_nodes]
        )
        ax.plot(x, totals, marker="o", markersize=6, linewidth=2, color=MODE_LINE_COLOR[mode],
                label=MODE_LABEL[mode])

    ax.set_xticks(x)
    ax.set_xticklabels([xtick_label(runs[(MODE_ORDER[0], n)] if (MODE_ORDER[0], n) in runs
                                     else runs[(MODE_ORDER[1], n)]) for n in all_nodes], fontsize=8)
    ax.set_ylabel("Total observed I/O time (s), write + read")
    ax.set_title("vpic_bdcats: total observed I/O time vs scale")
    ax.yaxis.grid(True, linestyle="--", alpha=0.4, zorder=0)
    ax.set_axisbelow(True)
    ax.legend(fontsize=9)

    fig.savefig(out_path, dpi=200)
    plt.close(fig)
    print(f"Wrote {out_path}")


def plot_throughput_line(runs, all_nodes, out_path):
    n_groups = len(all_nodes)
    x = np.arange(n_groups)
    fig, ax = plt.subplots(figsize=(max(6.5, 1.4 * n_groups), 5.5), constrained_layout=True)

    for mode in MODE_ORDER:
        for direction, style, marker in (("write", "-", "o"), ("read", "--", "s")):
            vals = np.array(
                [runs[(mode, n)]["throughput_mbps"][direction] / 1e3 if (mode, n) in runs else np.nan
                 for n in all_nodes]
            )
            ax.plot(x, vals, style, marker=marker, markersize=6, linewidth=2,
                    color=MODE_LINE_COLOR[mode], label=f"{MODE_LABEL[mode]} {direction}")

    ax.set_xticks(x)
    ax.set_xticklabels([xtick_label(runs[(MODE_ORDER[0], n)] if (MODE_ORDER[0], n) in runs
                                     else runs[(MODE_ORDER[1], n)]) for n in all_nodes], fontsize=8)
    ax.set_ylabel("Aggregate throughput (GB/s)")
    ax.set_title("vpic_bdcats: throughput vs scale (total bytes / total observed time)")
    ax.yaxis.grid(True, linestyle="--", alpha=0.4, zorder=0)
    ax.set_axisbelow(True)
    ax.legend(fontsize=9, ncol=2)

    fig.savefig(out_path, dpi=200)
    plt.close(fig)
    print(f"Wrote {out_path}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dirs", nargs="*", default=["."],
                     help="Directories to search for vpic_bdcats_<mode>_<nodes>.csv (default: .)")
    ap.add_argument("-o", "--out-dir", default=".", help="Directory to write PNGs into (default: .)")
    args = ap.parse_args()

    runs = load_all(args.dirs)
    if not runs:
        raise SystemExit(f"No vpic_bdcats_<mode>_<nodes>.csv files found under {args.dirs}")

    all_nodes = sorted({n for (_, n) in runs})
    os.makedirs(args.out_dir, exist_ok=True)

    plot_stacked_bar(runs, all_nodes, os.path.join(args.out_dir, "vpic_bdcats_stacked_bar.png"))
    plot_total_time_line(runs, all_nodes, os.path.join(args.out_dir, "vpic_bdcats_total_time.png"))
    plot_throughput_line(runs, all_nodes, os.path.join(args.out_dir, "vpic_bdcats_throughput.png"))


if __name__ == "__main__":
    main()
