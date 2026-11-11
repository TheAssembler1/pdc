#!/usr/bin/env python3
"""Turn tuolumne_ckpt_bench.sh results.csv into the same Markdown tables posted on PR #305.

usage: tuolumne_ckpt_summary.py results.csv [more.csv ...] > summary.md
"""
import csv
import sys
from collections import defaultdict

METRICS = [
    ("ckpt1_s", "Checkpoint Time (seconds)"),
    ("close1_s", "Server Close Time (seconds)"),
    ("restart_s", "Server Restart Time (seconds)"),
]
SUITE_TITLE = {
    "strong": "Strong Scaling (fixed {nobj} objects / {ntag} tags)",
    "weak": "Weak Scaling (fixed work per server)",
}


def fnum(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


def stats(vals):
    vals = [v for v in vals if v is not None]
    if not vals:
        return None
    return min(vals), sum(vals) / len(vals), max(vals), len(vals)


def fmt(st, i):
    return "NA" if st is None else f"{st[i]:.6f}"


def kfmt(n):
    n = int(n)
    return f"{n // 1000}K" if n % 1000 == 0 else str(n)


def main(paths):
    rows = []
    for p in paths:
        with open(p, newline="") as f:
            rows.extend(csv.DictReader(f))
    if not rows:
        sys.exit("no rows")

    ok = [r for r in rows if r["add_rc"] == "0" and r["verify_rc"] == "0"]
    bad = [r for r in rows if r not in ok]

    # key: (suite, n_servers, n_clients, n_obj, n_tag) -> variant -> metric -> [values]
    data = defaultdict(lambda: defaultdict(lambda: defaultdict(list)))
    for r in ok:
        key = (r["suite"], int(r["n_servers"]), int(r["n_clients"]), int(r["n_obj"]), int(r["n_tag"]))
        for m in ("ckpt1_s", "close1_s", "restart_s", "ckpt2_s", "close2_s"):
            data[key][r["variant"]][m].append(fnum(r[m]))

    variants = [v for v in ("prev", "new") if any(v in d for d in data.values())]
    vname = {"prev": "Prev", "new": "New"}
    out = []

    for suite in ("strong", "weak"):
        keys = sorted(k for k in data if k[0] == suite)
        if not keys:
            continue
        title = SUITE_TITLE[suite].format(nobj=kfmt(keys[0][3]), ntag=keys[0][4])
        out.append(f"## {title}\n")
        for metric, mtitle in METRICS:
            out.append(f"### {mtitle}\n")
            hdr = ["Servers"]
            for v in variants:
                hdr += [f"{vname[v]} Min", f"{vname[v]} Avg", f"{vname[v]} Max"]
            if len(variants) == 2:
                hdr.append("New/Prev (avg)")
            out.append("| " + " | ".join(hdr) + " |")
            out.append("|" + "---|" * len(hdr))
            for k in keys:
                label = f"-n {k[1]} -c 2"
                if suite == "weak":
                    label += f" ({kfmt(k[3])}/{k[4]})"
                label += f", {k[2]} clients"
                cells = [label]
                sts = {}
                for v in variants:
                    st = stats(data[k][v][metric])
                    sts[v] = st
                    cells += [fmt(st, 0), fmt(st, 1), fmt(st, 2)]
                if len(variants) == 2:
                    a, b = sts["prev"], sts["new"]
                    cells.append(f"{b[1] / a[1]:.2f}x" if a and b and a[1] > 0 else "NA")
                out.append("| " + " | ".join(cells) + " |")
            out.append("")

    # 2nd vs 1st checkpoint (PR's "Unexpected Checkpoint Overhead After Restart" table)
    for metric1, metric2, what in (("ckpt1_s", "ckpt2_s", "Checkpoint"), ("close1_s", "close2_s", "Server Close")):
        keys = sorted(k for k in data if k[0] == "strong")
        if not keys:
            break
        for v in variants:
            out.append(f"## {vname[v]}: 1st vs 2nd {what} Time, strong scaling (seconds)\n")
            out.append(f"| Servers | 1st {what} (Avg) | 2nd {what} (Avg) | Overhead (%) |")
            out.append("|---|---|---|---|")
            for k in keys:
                s1, s2 = stats(data[k][v][metric1]), stats(data[k][v][metric2])
                ov = f"{(s2[1] / s1[1] - 1) * 100:+.2f}%" if s1 and s2 and s1[1] > 0 else "NA"
                out.append(f"| -n {k[1]} -c 2 | {fmt(s1, 1)} | {fmt(s2, 1)} | {ov} |")
            out.append("")

    n_trials = sorted({len(m["ckpt1_s"]) for d in data.values() for m in d.values()})
    out.append(f"_Successful trials per cell: {', '.join(map(str, n_trials))}; failed trials: {len(bad)}._")
    for r in bad:
        out.append(
            f"- FAILED {r['variant']} {r['suite']} servers={r['n_servers']} obj={r['n_obj']} "
            f"trial={r['trial']} add_rc={r['add_rc']} verify_rc={r['verify_rc']}"
        )
    print("\n".join(out))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    main(sys.argv[1:])
