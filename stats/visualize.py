#!/usr/bin/env python3
"""
Generate comparison plots from stats/results/comparison.csv.

Usage:
    cd <repo-root>
    python3 stats/visualize.py [--csv <path>] [--out-dir <dir>]

Produces (in --out-dir, default stats/results/plots/):
  01_color_count_natural.png   – grouped bars: colors per tool, natural ordering
  02_color_count_largest.png   – grouped bars: colors per tool, largest-first ordering
  03_timing_large.png          – grouped bars: coloring time on large matrices
  04_nnz_vs_time.png           – scatter: nnz vs coloring time per tool (log-log)
  05_color_ratio_heatmap.png   – heatmap: PreCol color count / other-tool count
  06_ordering_effect.png       – line plot: ordering impact on color count per tool
"""

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import numpy as np

# ── palette ───────────────────────────────────────────────────────────────────

TOOL_COLORS = {
    "PreCol":  "#2196F3",   # blue
    "colpack": "#FF9800",   # orange
    "dsjm":    "#4CAF50",   # green
    "julia":   "#9C27B0",   # purple
}
TOOL_ORDER  = ["PreCol", "colpack", "dsjm", "julia"]
TOOL_LABELS = {"PreCol": "PreCol", "colpack": "ColPack",
               "dsjm": "DSJM", "julia": "Julia"}

# ── data loading ──────────────────────────────────────────────────────────────

def load_csv(path):
    rows = []
    with open(path) as f:
        for r in csv.DictReader(f):
            r["nnz_int"] = int(r["nnz"]) if r["nnz"].isdigit() else 0
            try:
                r["num_colors_int"] = int(r["num_colors"])
            except ValueError:
                r["num_colors_int"] = None
            try:
                r["time_ms_float"] = float(r["time_ms"])
            except ValueError:
                r["time_ms_float"] = None
            rows.append(r)
    return rows

def index(rows, side, ordering):
    """Return {matrix: {tool: (num_colors, time_ms)}} for given side+ordering."""
    d = defaultdict(dict)
    for r in rows:
        if r["side"] != side:
            continue
        if r["ordering"] != ordering:
            continue
        if r["num_colors_int"] is None:
            continue
        tool = r["tool"]
        d[r["matrix"]][tool] = (r["num_colors_int"], r["time_ms_float"])
    return d

# ── plot helpers ──────────────────────────────────────────────────────────────

def short_name(name):
    return name.replace(".mtx", "")

def grouped_bar(ax, matrices, data, tools, value_fn, ylabel, title, log=False):
    """
    matrices: list of matrix names (x-axis)
    data:     {matrix: {tool: (colors, time)}}
    value_fn: lambda (colors, time) -> float
    """
    n, k = len(matrices), len(tools)
    x    = np.arange(n)
    w    = 0.8 / k

    for i, tool in enumerate(tools):
        vals = []
        for m in matrices:
            entry = data.get(m, {}).get(tool)
            vals.append(value_fn(entry) if entry else float("nan"))
        bars = ax.bar(x + (i - k/2 + 0.5) * w, vals, w,
                      label=TOOL_LABELS[tool], color=TOOL_COLORS[tool],
                      alpha=0.85, edgecolor="white", linewidth=0.4)

    ax.set_xticks(x)
    ax.set_xticklabels([short_name(m) for m in matrices],
                       rotation=45, ha="right", fontsize=7)
    ax.set_ylabel(ylabel, fontsize=9)
    ax.set_title(title, fontsize=10, fontweight="bold")
    ax.legend(fontsize=8, ncol=2)
    ax.grid(axis="y", linestyle="--", alpha=0.4)
    if log:
        ax.set_yscale("log")
        ax.yaxis.set_minor_formatter(mticker.NullFormatter())

# ── individual plots ──────────────────────────────────────────────────────────

def plot_color_count(rows, ordering, dsjm_ord, out_path, title_suffix):
    # Gather data: tools that have the requested ordering
    # DSJM uses its own labels, so we pull it separately
    d_main  = index(rows, "column", ordering)
    d_dsjm  = index(rows, "column", dsjm_ord)

    matrices = sorted(d_main.keys(), key=short_name)
    if not matrices:
        return

    # Merge dsjm into d_main
    merged = {}
    for m in matrices:
        merged[m] = dict(d_main.get(m, {}))
        dsjm_entry = d_dsjm.get(m, {}).get("dsjm")
        if dsjm_entry:
            merged[m]["dsjm"] = dsjm_entry

    # Only keep matrices where all 4 tools have a result
    valid = [m for m in matrices
             if all(merged.get(m, {}).get(t) is not None for t in TOOL_ORDER)]

    if not valid:
        valid = [m for m in matrices
                 if any(merged.get(m, {}).get(t) is not None for t in TOOL_ORDER)]

    fig, ax = plt.subplots(figsize=(max(14, len(valid) * 0.55), 5))
    grouped_bar(ax, valid, merged, TOOL_ORDER,
                lambda e: e[0],
                "Number of colors",
                f"Column coloring — color count ({title_suffix})")
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"  saved {out_path}")


def plot_timing_large(rows, out_path, nnz_threshold=200_000):
    """Bar chart of coloring time on matrices with nnz > threshold."""
    large = [r for r in rows
             if r["nnz_int"] >= nnz_threshold
             and r["side"] == "column"
             and r["ordering"] in ("natural", "lfo")
             and r["time_ms_float"] is not None
             and r["time_ms_float"] > 0]

    d = defaultdict(dict)
    for r in large:
        d[r["matrix"]][r["tool"]] = (r["num_colors_int"], r["time_ms_float"])

    matrices = sorted(d.keys(), key=lambda m: -max(
        v[1] for v in d[m].values() if v[1]))

    fig, ax = plt.subplots(figsize=(max(12, len(matrices) * 0.8), 5))
    grouped_bar(ax, matrices, d, TOOL_ORDER,
                lambda e: e[1] if e[1] else float("nan"),
                "Coloring time (ms, log scale)",
                f"Coloring time — large matrices (nnz ≥ {nnz_threshold:,})",
                log=True)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"  saved {out_path}")


def plot_nnz_vs_time(rows, out_path):
    """Log-log scatter: nnz vs coloring time, one series per tool."""
    fig, ax = plt.subplots(figsize=(8, 6))

    for tool in TOOL_ORDER:
        pts = [(r["nnz_int"], r["time_ms_float"])
               for r in rows
               if r["tool"] == tool
               and r["side"] == "column"
               and r["ordering"] in ("natural", "lfo")
               and r["nnz_int"] > 0
               and r["time_ms_float"] is not None
               and r["time_ms_float"] > 0]
        if not pts:
            continue
        xs, ys = zip(*pts)
        ax.scatter(xs, ys, label=TOOL_LABELS[tool],
                   color=TOOL_COLORS[tool], alpha=0.7, s=35, edgecolors="white",
                   linewidths=0.4)

        # Trend line (log-log linear regression)
        lx = np.log10(xs)
        ly = np.log10(ys)
        if len(lx) >= 3:
            m, b = np.polyfit(lx, ly, 1)
            xr = np.linspace(min(lx), max(lx), 100)
            ax.plot(10**xr, 10**(m * xr + b),
                    color=TOOL_COLORS[tool], linewidth=1.2, alpha=0.6)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Non-zeros (nnz)", fontsize=10)
    ax.set_ylabel("Coloring time (ms)", fontsize=10)
    ax.set_title("nnz vs coloring time — column / natural ordering\n(dashed = log-log trend)",
                 fontsize=10, fontweight="bold")
    ax.legend(fontsize=9)
    ax.grid(True, which="both", linestyle="--", alpha=0.3)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"  saved {out_path}")


def plot_color_ratio_heatmap(rows, out_path):
    """
    Heatmap: ratio = tool_colors / PreCol_colors.
    Cells > 1 mean the tool uses more colors than PreCol.
    """
    d_nat   = index(rows, "column", "natural")
    d_lfo   = index(rows, "column", "lfo")   # for dsjm

    matrices = sorted(set(d_nat) | set(d_lfo), key=short_name)
    comp_tools = ["colpack", "dsjm", "julia"]

    ratios = np.full((len(comp_tools), len(matrices)), float("nan"))

    for j, m in enumerate(matrices):
        precol_nc = (d_nat.get(m, {}).get("PreCol") or (None,))[0]
        if not precol_nc:
            continue
        for i, tool in enumerate(comp_tools):
            src = d_lfo if tool == "dsjm" else d_nat
            entry = src.get(m, {}).get(tool)
            if entry and entry[0]:
                ratios[i, j] = entry[0] / precol_nc

    # Drop columns that are all NaN
    valid_cols = [j for j in range(len(matrices))
                  if not np.all(np.isnan(ratios[:, j]))]
    matrices   = [matrices[j] for j in valid_cols]
    ratios     = ratios[:, valid_cols]

    fig, ax = plt.subplots(figsize=(max(14, len(matrices) * 0.4), 3.5))
    vmax = max(2.0, float(np.nanmax(ratios)) if not np.all(np.isnan(ratios)) else 2.0)
    vmin = min(0.5, float(np.nanmin(ratios)) if not np.all(np.isnan(ratios)) else 0.5)

    im = ax.imshow(ratios, aspect="auto", cmap="RdYlGn_r",
                   vmin=vmin, vmax=vmax)
    plt.colorbar(im, ax=ax, label="ratio vs PreCol (1.0 = same)")

    ax.set_yticks(range(len(comp_tools)))
    ax.set_yticklabels([TOOL_LABELS[t] for t in comp_tools], fontsize=9)
    ax.set_xticks(range(len(matrices)))
    ax.set_xticklabels([short_name(m) for m in matrices],
                       rotation=45, ha="right", fontsize=7)
    ax.set_title("Color count ratio relative to PreCol (natural ordering)\n"
                 "Green < 1 = fewer colors than PreCol; Red > 1 = more colors",
                 fontsize=10, fontweight="bold")

    # Annotate cells
    for i in range(len(comp_tools)):
        for j in range(len(matrices)):
            v = ratios[i, j]
            if not math.isnan(v):
                ax.text(j, i, f"{v:.2f}", ha="center", va="center",
                        fontsize=5.5, color="black")

    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"  saved {out_path}")


def plot_ordering_effect(rows, out_path):
    """
    Line plot: for each tool, show median color count across matrices
    for each ordering (natural / largest_first / smallest_last).
    """
    orderings_map = {
        "PreCol":  ["natural", "largest_first", "smallest_last"],
        "colpack": ["natural", "largest_first", "smallest_last"],
        "julia":   ["natural", "largest_first", "smallest_last"],
        "dsjm":    ["lfo",     "slo",           "ido"],
    }
    ordering_labels = {
        "natural":       "Natural",
        "largest_first": "Largest first",
        "smallest_last": "Smallest last",
        "lfo":           "LFO",
        "slo":           "SLO",
        "ido":           "IDO",
    }

    fig, ax = plt.subplots(figsize=(8, 5))
    x = [0, 1, 2]
    x_labels = ["Natural / LFO", "Largest-first / SLO", "Smallest-last / IDO"]

    for tool in TOOL_ORDER:
        ords = orderings_map[tool]
        medians = []
        for o in ords:
            vals = [r["num_colors_int"]
                    for r in rows
                    if r["tool"] == tool
                    and r["side"] == "column"
                    and r["ordering"] == o
                    and r["num_colors_int"] is not None]
            medians.append(float(np.median(vals)) if vals else float("nan"))
        ax.plot(x, medians, marker="o", label=TOOL_LABELS[tool],
                color=TOOL_COLORS[tool], linewidth=2, markersize=7)

    ax.set_xticks(x)
    ax.set_xticklabels(x_labels, fontsize=9)
    ax.set_ylabel("Median color count across all matrices", fontsize=9)
    ax.set_title("Effect of vertex ordering on color count (column coloring)",
                 fontsize=10, fontweight="bold")
    ax.legend(fontsize=9)
    ax.grid(axis="y", linestyle="--", alpha=0.4)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"  saved {out_path}")


# ── main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    repo_root = Path(__file__).resolve().parent.parent
    parser.add_argument("--csv",     default=str(repo_root / "stats/results/comparison.csv"))
    parser.add_argument("--out-dir", default=str(repo_root / "stats/results/plots"))
    args = parser.parse_args()

    csv_path = Path(args.csv)
    if not csv_path.exists():
        print(f"CSV not found: {csv_path}\nRun stats/compare.py first.")
        raise SystemExit(1)

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    rows = load_csv(csv_path)
    print(f"Loaded {len(rows)} rows from {csv_path}")
    print(f"Saving plots to {out_dir}/")

    plot_color_count(rows, "natural", "lfo",
                     out_dir / "01_color_count_natural.png",
                     "natural ordering")

    plot_color_count(rows, "largest_first", "lfo",
                     out_dir / "02_color_count_largest.png",
                     "largest-first ordering")

    plot_timing_large(rows, out_dir / "03_timing_large.png")
    plot_nnz_vs_time(rows,  out_dir / "04_nnz_vs_time.png")
    plot_color_ratio_heatmap(rows, out_dir / "05_color_ratio_heatmap.png")
    plot_ordering_effect(rows,     out_dir / "06_ordering_effect.png")

    print("Done.")


if __name__ == "__main__":
    main()
