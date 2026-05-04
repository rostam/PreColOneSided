#!/usr/bin/env python3
"""
Generate comparison plots from stats/results/comparison.csv.

All bar/heatmap plots focus on large matrices (nnz >= NNZ_THRESHOLD).

Usage:
    cd <repo-root>
    python3 stats/visualize.py [--csv <path>] [--out-dir <dir>]

Produces (in --out-dir, default stats/results/plots/):
  01_color_count_natural.png   – grouped bars: colors per tool, natural ordering
  02_color_count_largest.png   – grouped bars: colors per tool, largest-first ordering
  03_timing.png                – grouped bars: coloring time (log scale)
  04_nnz_vs_time.png           – log-log scatter: nnz vs coloring time
  05_color_ratio_heatmap.png   – heatmap: ratio of each tool's colors vs PreCol
  06_ordering_effect.png       – median color count vs ordering per tool
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

# ── constants ─────────────────────────────────────────────────────────────────

NNZ_THRESHOLD = 200_000   # "large" matrices

TOOL_ORDER  = ["PreCol", "colpack", "dsjm", "julia"]
TOOL_LABELS = {"PreCol": "PreCol", "colpack": "ColPack",
               "dsjm": "DSJM",    "julia": "Julia"}
TOOL_COLORS = {"PreCol": "#2196F3", "colpack": "#FF9800",
               "dsjm":   "#4CAF50", "julia":   "#9C27B0"}

# ── data loading ──────────────────────────────────────────────────────────────

def load_csv(path):
    rows = []
    with open(path) as f:
        for r in csv.DictReader(f):
            try:
                r["nnz_int"] = int(r["nnz"])
            except ValueError:
                r["nnz_int"] = 0
            try:
                r["nc"] = int(r["num_colors"])
            except ValueError:
                r["nc"] = None
            try:
                r["t"] = float(r["time_ms"])
            except ValueError:
                r["t"] = None
            rows.append(r)
    return rows

def pivot(rows, side, ordering, value="nc"):
    """
    Return {matrix: {tool: value}} for the given side + ordering.
    DSJM ordering aliases: natural→lfo, largest_first→lfo, smallest_last→slo.
    """
    DSJM_ALIAS = {"natural": "lfo", "largest_first": "lfo", "smallest_last": "slo"}
    result = defaultdict(dict)
    for r in rows:
        if r["side"] != side:
            continue
        tool = r["tool"]
        expected = DSJM_ALIAS.get(ordering, ordering) if tool == "dsjm" else ordering
        if r["ordering"] != expected:
            continue
        v = r[value]
        if v is not None:
            result[r["matrix"]][tool] = v
    return result

def large_matrices(rows):
    return sorted(
        {r["matrix"] for r in rows if r["nnz_int"] >= NNZ_THRESHOLD},
        key=lambda m: m.replace(".mtx", "")
    )

def short(name):
    return name.replace(".mtx", "")

# ── grouped bar helper ────────────────────────────────────────────────────────

def grouped_bar(ax, matrices, data, ylabel, title, log=False):
    n, k = len(matrices), len(TOOL_ORDER)
    x = np.arange(n)
    w = 0.7 / k

    for i, tool in enumerate(TOOL_ORDER):
        vals = [data.get(m, {}).get(tool, float("nan")) for m in matrices]
        ax.bar(x + (i - k / 2 + 0.5) * w, vals, w,
               label=TOOL_LABELS[tool], color=TOOL_COLORS[tool],
               alpha=0.88, edgecolor="white", linewidth=0.5)

    ax.set_xticks(x)
    ax.set_xticklabels([short(m) for m in matrices],
                       rotation=40, ha="right", fontsize=8)
    ax.set_ylabel(ylabel, fontsize=10)
    ax.set_title(title, fontsize=11, fontweight="bold", pad=10)
    ax.legend(fontsize=9, ncol=2, loc="upper right")
    ax.grid(axis="y", linestyle="--", alpha=0.35)
    if log:
        ax.set_yscale("log")
        ax.yaxis.set_minor_formatter(mticker.NullFormatter())

# ── plot 01 & 02: color counts (natural / largest-first) ─────────────────────

def plot_color_count(rows, ordering, out_path, title_suffix):
    d   = pivot(rows, "column", ordering, "nc")
    mats = [m for m in large_matrices(rows) if m in d]

    fig, ax = plt.subplots(figsize=(max(10, len(mats) * 0.75), 5))
    grouped_bar(ax, mats, d,
                "Number of colors",
                f"Column coloring — color count  ({title_suffix})\n"
                f"Large matrices only  (nnz ≥ {NNZ_THRESHOLD:,})")
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"  saved {out_path.name}")

# ── plot 03: timing ───────────────────────────────────────────────────────────

def plot_timing(rows, out_path):
    d   = pivot(rows, "column", "natural", "t")
    mats = [m for m in large_matrices(rows) if m in d]
    # sort by PreCol time descending
    mats.sort(key=lambda m: -(d.get(m, {}).get("PreCol") or 0))

    fig, ax = plt.subplots(figsize=(max(10, len(mats) * 0.75), 5))
    grouped_bar(ax, mats, d,
                "Coloring time (ms, log scale)",
                f"Column coloring — timing  (natural ordering)\n"
                f"Large matrices only  (nnz ≥ {NNZ_THRESHOLD:,})",
                log=True)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"  saved {out_path.name}")

# ── plot 04: nnz vs time scatter (all matrices) ───────────────────────────────

def plot_nnz_vs_time(rows, out_path):
    fig, ax = plt.subplots(figsize=(8, 6))

    for tool in TOOL_ORDER:
        pts = [
            (r["nnz_int"], r["t"])
            for r in rows
            if r["tool"] == tool
            and r["side"] == "column"
            and r["ordering"] in ("natural", "lfo")
            and r["nnz_int"] > 0
            and r["t"] is not None and r["t"] > 0
        ]
        if not pts:
            continue
        xs, ys = zip(*pts)
        ax.scatter(xs, ys, label=TOOL_LABELS[tool],
                   color=TOOL_COLORS[tool], alpha=0.72, s=40,
                   edgecolors="white", linewidths=0.4)
        # log-log trend line
        lx, ly = np.log10(xs), np.log10(ys)
        if len(lx) >= 3:
            m, b = np.polyfit(lx, ly, 1)
            xr = np.linspace(min(lx), max(lx), 120)
            ax.plot(10**xr, 10**(m * xr + b),
                    color=TOOL_COLORS[tool], linewidth=1.5, alpha=0.55,
                    linestyle="--")

    # vertical line at threshold
    ax.axvline(NNZ_THRESHOLD, color="gray", linestyle=":", linewidth=1,
               label=f"nnz = {NNZ_THRESHOLD:,}")

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Non-zeros (nnz)", fontsize=10)
    ax.set_ylabel("Coloring time (ms)", fontsize=10)
    ax.set_title("nnz vs coloring time — column / natural ordering\n"
                 "(dashed = log-log trend line; all matrices)",
                 fontsize=10, fontweight="bold")
    ax.legend(fontsize=9)
    ax.grid(True, which="both", linestyle="--", alpha=0.25)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"  saved {out_path.name}")

# ── plot 05: color ratio heatmap ──────────────────────────────────────────────

def plot_color_ratio_heatmap(rows, out_path):
    d_nat = pivot(rows, "column", "natural", "nc")   # all tools natural
    mats  = [m for m in large_matrices(rows) if m in d_nat]

    comp_tools = ["colpack", "dsjm", "julia"]
    ratios = np.full((len(comp_tools), len(mats)), float("nan"))

    for j, m in enumerate(mats):
        precol = d_nat.get(m, {}).get("PreCol")
        if not precol:
            continue
        for i, tool in enumerate(comp_tools):
            v = d_nat.get(m, {}).get(tool)
            if v:
                ratios[i, j] = v / precol

    all_vals = ratios[~np.isnan(ratios)]
    vmin = max(0.4, float(np.min(all_vals))) if len(all_vals) else 0.4
    vmax = min(3.0, float(np.max(all_vals))) if len(all_vals) else 3.0

    fig, ax = plt.subplots(figsize=(max(10, len(mats) * 0.72), 3.5))
    im = ax.imshow(ratios, aspect="auto", cmap="RdYlGn_r", vmin=vmin, vmax=vmax)
    cbar = plt.colorbar(im, ax=ax, fraction=0.03, pad=0.02)
    cbar.set_label("colors / PreCol colors  (1.0 = same)", fontsize=9)

    ax.set_yticks(range(len(comp_tools)))
    ax.set_yticklabels([TOOL_LABELS[t] for t in comp_tools], fontsize=10)
    ax.set_xticks(range(len(mats)))
    ax.set_xticklabels([short(m) for m in mats],
                       rotation=40, ha="right", fontsize=8)
    ax.set_title("Color count ratio vs PreCol (natural ordering)\n"
                 "Green < 1.0 = fewer colors than PreCol;  Red > 1.0 = more colors",
                 fontsize=10, fontweight="bold", pad=10)

    for i in range(len(comp_tools)):
        for j in range(len(mats)):
            v = ratios[i, j]
            if not math.isnan(v):
                ax.text(j, i, f"{v:.2f}", ha="center", va="center",
                        fontsize=7, color="black")

    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"  saved {out_path.name}")

# ── plot 06: ordering effect ──────────────────────────────────────────────────

def plot_ordering_effect(rows, out_path):
    # (tool, ordering_in_csv, x_position, x_label)
    steps = [
        ("PreCol",  "natural",       0, "Natural"),
        ("PreCol",  "largest_first", 1, "Largest first"),
        ("PreCol",  "smallest_last", 2, "Smallest last"),
    ]
    orderings_per_tool = {
        "PreCol":  ["natural",       "largest_first", "smallest_last"],
        "colpack": ["natural",       "largest_first", "smallest_last"],
        "julia":   ["natural",       "largest_first", "smallest_last"],
        "dsjm":    ["lfo",           "slo",           "ido"],
    }
    x_labels = ["Natural / LFO", "Largest-first / SLO", "Smallest-last / IDO"]

    lmats = large_matrices(rows)

    fig, axes = plt.subplots(1, 2, figsize=(13, 5), sharey=False)

    for ax, metric, ylabel, value_key in [
        (axes[0], "nc", "Median color count", "nc"),
        (axes[1], "t",  "Median coloring time (ms)", "t"),
    ]:
        for tool in TOOL_ORDER:
            ords = orderings_per_tool[tool]
            medians = []
            for o in ords:
                vals = [
                    r[value_key]
                    for r in rows
                    if r["tool"] == tool
                    and r["side"] == "column"
                    and r["ordering"] == o
                    and r["matrix"] in lmats
                    and r[value_key] is not None
                    and (value_key != "t" or r[value_key] > 0)
                ]
                medians.append(float(np.median(vals)) if vals else float("nan"))

            ax.plot([0, 1, 2], medians, marker="o",
                    label=TOOL_LABELS[tool], color=TOOL_COLORS[tool],
                    linewidth=2.2, markersize=8)

        ax.set_xticks([0, 1, 2])
        ax.set_xticklabels(x_labels, fontsize=9)
        ax.set_ylabel(ylabel, fontsize=10)
        ax.legend(fontsize=9)
        ax.grid(axis="y", linestyle="--", alpha=0.35)

    axes[0].set_title("Median color count vs ordering\n"
                      f"(large matrices, nnz ≥ {NNZ_THRESHOLD:,})",
                      fontsize=10, fontweight="bold")
    axes[1].set_title("Median coloring time vs ordering\n"
                      f"(large matrices, nnz ≥ {NNZ_THRESHOLD:,})",
                      fontsize=10, fontweight="bold")

    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"  saved {out_path.name}")

# ── plot 07: restricted vs regular ───────────────────────────────────────────

def plot_restricted_comparison(rows, out_path):
    """
    Two sub-plots for large matrices, natural column ordering:
      Left:  color counts  — PreCol (regular) + restricted at block sizes 4, 16, 64
      Right: % color reduction = (regular - restricted) / regular * 100
    """
    BLOCK_SIZES   = [4, 16, 64]
    BLOCK_COLORS  = {"4": "#64B5F6", "16": "#1565C0", "64": "#0D47A1"}
    REG_COLOR     = TOOL_COLORS["PreCol"]

    lmats = large_matrices(rows)

    # Build lookup: {matrix: {tool: (nc, t)}}
    d = defaultdict(dict)
    for r in rows:
        if r["side"] != "column" or r["ordering"] not in ("natural",):
            continue
        if r["nc"] is None:
            continue
        d[r["matrix"]][r["tool"]] = r["nc"]

    mats = [m for m in lmats if "PreCol" in d.get(m, {})]
    if not mats:
        print("  no data for restricted comparison plot — skipping")
        return

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(max(10, len(mats) * 0.75), 9),
                                   sharex=True)

    x  = np.arange(len(mats))
    n_series = 1 + len(BLOCK_SIZES)
    w  = 0.75 / n_series

    # ── top: absolute color counts ────────────────────────────────────────────
    reg_vals = [d.get(m, {}).get("PreCol", float("nan")) for m in mats]
    ax1.bar(x - (n_series / 2 - 0.5) * w, reg_vals, w,
            label="PreCol (regular)", color=REG_COLOR, alpha=0.9,
            edgecolor="white", linewidth=0.5)

    for j, bs in enumerate(BLOCK_SIZES):
        tool = f"PreCol-restricted-{bs}"
        vals = [d.get(m, {}).get(tool, float("nan")) for m in mats]
        ax1.bar(x + (j - n_series / 2 + 1.5) * w, vals, w,
                label=f"Restricted block={bs}", color=BLOCK_COLORS[str(bs)],
                alpha=0.88, edgecolor="white", linewidth=0.5)

    ax1.set_ylabel("Number of colors", fontsize=10)
    ax1.set_title(f"Regular vs restricted coloring — large matrices  (nnz ≥ {NNZ_THRESHOLD:,})\n"
                  "Column coloring, natural ordering",
                  fontsize=11, fontweight="bold", pad=8)
    ax1.legend(fontsize=8, ncol=2, loc="upper right")
    ax1.grid(axis="y", linestyle="--", alpha=0.35)

    # ── bottom: % reduction ───────────────────────────────────────────────────
    for j, bs in enumerate(BLOCK_SIZES):
        tool = f"PreCol-restricted-{bs}"
        reductions = []
        for m in mats:
            reg = d.get(m, {}).get("PreCol")
            res = d.get(m, {}).get(tool)
            if reg and res:
                reductions.append(100.0 * (reg - res) / reg)
            else:
                reductions.append(float("nan"))
        ax2.bar(x + (j - len(BLOCK_SIZES) / 2 + 0.5) * (0.7 / len(BLOCK_SIZES)),
                reductions, 0.7 / len(BLOCK_SIZES),
                label=f"block={bs}", color=BLOCK_COLORS[str(bs)],
                alpha=0.88, edgecolor="white", linewidth=0.5)

    ax2.axhline(0, color="black", linewidth=0.8)
    ax2.set_ylabel("Color reduction (%)", fontsize=10)
    ax2.set_title("Color count reduction: (regular − restricted) / regular × 100",
                  fontsize=10, fontweight="bold")
    ax2.legend(fontsize=9, loc="upper right")
    ax2.grid(axis="y", linestyle="--", alpha=0.35)

    ax2.set_xticks(x)
    ax2.set_xticklabels([short(m) for m in mats],
                        rotation=40, ha="right", fontsize=8)

    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"  saved {out_path.name}")

# ── main ──────────────────────────────────────────────────────────────────────

def main():
    repo_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
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
    lmats = large_matrices(rows)
    print(f"Loaded {len(rows)} rows — {len(lmats)} large matrices (nnz ≥ {NNZ_THRESHOLD:,})")
    print(f"Saving plots to {out_dir}/\n")

    plot_color_count(rows, "natural",       out_dir / "01_color_count_natural.png",
                     "natural ordering")
    plot_color_count(rows, "largest_first", out_dir / "02_color_count_largest.png",
                     "largest-first ordering")
    plot_timing(rows,                       out_dir / "03_timing.png")
    plot_nnz_vs_time(rows,                  out_dir / "04_nnz_vs_time.png")
    plot_color_ratio_heatmap(rows,          out_dir / "05_color_ratio_heatmap.png")
    plot_ordering_effect(rows,              out_dir / "06_ordering_effect.png")
    plot_restricted_comparison(rows,        out_dir / "07_restricted_comparison.png")

    print("\nDone.")


if __name__ == "__main__":
    main()
