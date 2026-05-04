#!/usr/bin/env python3
"""
Comparison of one-sided partial distance-2 graph coloring tools:
  - precoloring  (this project)
  - ColPack
  - DSJM gcolor
  - Julia SparseMatrixColorings.jl

Usage:
    cd <repo-root>
    python3 stats/compare.py [--mtx-dir <path>] [--out <csv>] [--timeout <sec>]

Outputs a CSV to --out (default: stats/results/comparison.csv) and prints
a summary table to stdout.
"""

import argparse
import csv
import os
import re
import subprocess
import sys
import time
from pathlib import Path

# ── tool paths ────────────────────────────────────────────────────────────────

REPO_ROOT   = Path(__file__).resolve().parent.parent
STATS_DIR   = REPO_ROOT / "stats"
PRECOLORING = REPO_ROOT / "build" / "precoloring"
COLPACK     = STATS_DIR / "ColPack"
DSJM        = Path.home() / "Downloads" / "dsjm" / "DSJM" / "examples" / "gcolor"
JULIA_BIN   = Path("/home/rostam/.juliaup/bin/julia")
JULIA_SCRIPT = STATS_DIR / "julia_color.jl"

# ── ordering maps ──────────────────────────────────────────────────────────────

# (display name, precoloring flag, colpack flag, dsjm flag)
ORDERINGS = [
    ("natural",       "natural",  "NATURAL",       "lfo"),   # DSJM has no natural; lfo is closest default
    ("largest_first", "largest",  "LARGEST_FIRST", "lfo"),
    ("smallest_last", "smallest", "SMALLEST_LAST", "slo"),
]

SIDES = ["column", "row"]

# ColPack method names per side
COLPACK_METHOD = {"column": "COLUMN_PARTIAL_DISTANCE_TWO",
                  "row":    "ROW_PARTIAL_DISTANCE_TWO"}

# ── subprocess helpers ────────────────────────────────────────────────────────

def run(cmd, timeout, cwd=None):
    """Run a command; return (stdout, stderr, elapsed_wall_ms, returncode)."""
    t0 = time.perf_counter()
    try:
        proc = subprocess.run(
            cmd, capture_output=True, text=True, timeout=timeout, cwd=cwd
        )
        elapsed = (time.perf_counter() - t0) * 1000.0
        return proc.stdout, proc.stderr, elapsed, proc.returncode
    except subprocess.TimeoutExpired:
        return "", "TIMEOUT", timeout * 1000.0, -1

# ── parsers ───────────────────────────────────────────────────────────────────

def parse_precoloring(stdout):
    """Return (num_colors, time_ms) from precoloring output."""
    m_colors = re.search(r"Colors used:\s*(\d+)", stdout)
    m_time   = re.search(r"Coloring:\s*([\d.]+)\s*ms", stdout)
    colors = int(m_colors.group(1)) if m_colors else None
    t_ms   = float(m_time.group(1)) if m_time else None
    return colors, t_ms

def parse_colpack(stdout):
    """Return list of (ordering, method, num_colors, time_ms) from ColPack -v output."""
    results = []
    blocks = re.findall(
        r"order:\s*(\S+).*?methd:\s*(\S+).*?order\+color time\s*=\s*([\d.]+).*?number of colors:\s*(\d+)",
        stdout, re.DOTALL
    )
    for order, method, t_str, nc_str in blocks:
        results.append((order, method, int(nc_str), float(t_str) * 1000.0))
    return results

def parse_dsjm_stats(stdout):
    """
    Parse DSJM -s output.
    Line 1: '...path... rows & cols & nnz & NUMat row R'
    Line 2: 'c1 & c2 & c3 ...'  (color assigned to each column, 1-based)
    Returns num_colors or None.
    """
    # The number of colors appears as max of the color-assignment line (line 2)
    lines = [l.strip() for l in stdout.splitlines() if l.strip()]
    for line in lines:
        if re.search(r'^\d+(\s*&\s*\d+)+$', line):
            vals = [int(x) for x in re.split(r'\s*&\s*', line)]
            return max(vals)
    # fallback: try the inline pattern 'N & N & N & Nat row'
    m = re.search(r'&\s*(\d+)at\s+row', stdout)
    if m:
        return int(m.group(1))
    return None

# ── per-tool runners ──────────────────────────────────────────────────────────

def run_precoloring(mtx, side, ordering_flag, timeout):
    if not PRECOLORING.exists():
        return None, None, "binary not found"
    # CLI accepts "rows"/"columns" (plural)
    cli_side = "columns" if side == "column" else "rows"
    stdout, stderr, wall, rc = run(
        [str(PRECOLORING), str(mtx), "--side", cli_side, "--order", ordering_flag],
        timeout
    )
    colors, t = parse_precoloring(stdout)
    return colors, t, None if colors is not None else (stderr or stdout)[:120]


def run_colpack(mtx, side, colpack_order, timeout):
    if not COLPACK.exists():
        return [], "binary not found"
    method = COLPACK_METHOD[side]
    stdout, stderr, wall, rc = run(
        [str(COLPACK), "-f", str(mtx), "-o", colpack_order, "-m", method, "-v"],
        timeout, cwd=str(REPO_ROOT)
    )
    results = parse_colpack(stdout)
    if not results:
        return [], (stderr or stdout)[:120]
    return results, None


def run_dsjm(mtx, dsjm_order, timeout):
    """DSJM only does column-side coloring; run with -s to avoid segfault."""
    if not DSJM.exists():
        return None, None, "binary not found"
    stdout, stderr, wall, rc = run(
        [str(DSJM), "-s", "-i", str(mtx), "-m", dsjm_order],
        timeout
    )
    colors = parse_dsjm_stats(stdout)
    return colors, wall, None if colors is not None else (stderr or stdout)[:120]


def run_julia_batch(mtx_files, timeout):
    """Run Julia once over all matrices; return list of CSV-row dicts."""
    if not JULIA_BIN.exists() or not JULIA_SCRIPT.exists():
        return []
    cmd = [
        str(JULIA_BIN),
        f"--project={Path.home()}/.julia/environments/v1.11",
        str(JULIA_SCRIPT),
    ] + [str(f) for f in mtx_files]

    total_timeout = max(timeout * len(mtx_files), 600)
    stdout, stderr, wall, rc = run(cmd, total_timeout)

    rows = []
    for line in stdout.splitlines():
        if line.startswith("matrix,") or not line.strip():
            continue
        parts = line.split(",")
        if len(parts) == 9:
            rows.append({
                "matrix":     parts[0],
                "rows":       parts[1],
                "cols":       parts[2],
                "nnz":        parts[3],
                "tool":       parts[4],
                "side":       parts[5],
                "ordering":   parts[6],
                "num_colors": parts[7],
                "time_ms":    parts[8],
            })
    return rows

# ── matrix discovery ──────────────────────────────────────────────────────────

def get_matrix_info(mtx_path):
    """Quick parse of header + size line to get (rows, cols, nnz)."""
    try:
        with open(mtx_path) as f:
            for line in f:
                line = line.strip()
                if line.startswith("%"):
                    continue
                parts = line.split()
                if len(parts) >= 2:
                    return int(parts[0]), int(parts[1]), int(parts[2]) if len(parts) >= 3 else 0
    except Exception:
        pass
    return None, None, None

# ── main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--mtx-dir", default=str(REPO_ROOT / "FloridaSparseMatrixCollection"),
                        help="Directory with .mtx files")
    parser.add_argument("--out", default=str(STATS_DIR / "results" / "comparison.csv"),
                        help="Output CSV path")
    parser.add_argument("--timeout", type=float, default=120.0,
                        help="Per-tool per-matrix timeout in seconds (default: 120)")
    args = parser.parse_args()

    mtx_dir = Path(args.mtx_dir)
    mtx_files = sorted(mtx_dir.glob("*.mtx"))
    if not mtx_files:
        print(f"No .mtx files found in {mtx_dir}", file=sys.stderr)
        sys.exit(1)

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    CSV_FIELDS = ["matrix", "rows", "cols", "nnz",
                  "tool", "side", "ordering", "num_colors", "time_ms"]

    all_rows = []

    # ── Julia: run once for all matrices ─────────────────────────────────────
    print(f"[Julia] running batch over {len(mtx_files)} matrices (includes JIT warmup)…",
          flush=True)
    julia_rows = run_julia_batch(mtx_files, args.timeout)
    all_rows.extend(julia_rows)
    print(f"[Julia] collected {len(julia_rows)} rows", flush=True)

    # ── precoloring + ColPack + DSJM: per matrix ──────────────────────────────
    for i, mtx in enumerate(mtx_files):
        bname = mtx.name
        m_rows, m_cols, m_nnz = get_matrix_info(mtx)
        print(f"[{i+1}/{len(mtx_files)}] {bname}  ({m_rows}×{m_cols}, nnz={m_nnz})",
              flush=True)

        for side in SIDES:
            for ord_name, precol_ord, colpack_ord, dsjm_ord in ORDERINGS:

                # ── precoloring ──────────────────────────────────────────────
                colors, t, err = run_precoloring(mtx, side, precol_ord, args.timeout)
                all_rows.append({
                    "matrix":     bname,
                    "rows":       m_rows,
                    "cols":       m_cols,
                    "nnz":        m_nnz,
                    "tool":       "precoloring",
                    "side":       side,
                    "ordering":   ord_name,
                    "num_colors": colors if colors is not None else "ERROR",
                    "time_ms":    f"{t:.3f}" if t is not None else "ERROR",
                })

                # ── ColPack (one call per ordering per side) ─────────────────
                cp_results, cp_err = run_colpack(mtx, side, colpack_ord, args.timeout)
                if cp_results:
                    for cp_ord, cp_meth, cp_nc, cp_t in cp_results:
                        all_rows.append({
                            "matrix":     bname,
                            "rows":       m_rows,
                            "cols":       m_cols,
                            "nnz":        m_nnz,
                            "tool":       "colpack",
                            "side":       side,
                            "ordering":   cp_ord.lower(),
                            "num_colors": cp_nc,
                            "time_ms":    f"{cp_t:.3f}",
                        })
                else:
                    all_rows.append({
                        "matrix":     bname,
                        "rows":       m_rows,
                        "cols":       m_cols,
                        "nnz":        m_nnz,
                        "tool":       "colpack",
                        "side":       side,
                        "ordering":   ord_name,
                        "num_colors": "ERROR",
                        "time_ms":    "ERROR",
                    })

        # ── DSJM: column side only, its own orderings ────────────────────────
        for dsjm_m in ("lfo", "slo", "ido"):
            colors, wall, err = run_dsjm(mtx, dsjm_m, args.timeout)
            all_rows.append({
                "matrix":     bname,
                "rows":       m_rows,
                "cols":       m_cols,
                "nnz":        m_nnz,
                "tool":       "dsjm",
                "side":       "column",
                "ordering":   dsjm_m,
                "num_colors": colors if colors is not None else "ERROR",
                "time_ms":    f"{wall:.3f}" if wall is not None else "ERROR",
            })

    # ── write CSV ─────────────────────────────────────────────────────────────
    with open(out_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=CSV_FIELDS)
        writer.writeheader()
        writer.writerows(all_rows)
    print(f"\nCSV written to {out_path}")

    # ── summary table ─────────────────────────────────────────────────────────
    print_summary(all_rows, mtx_files)


def print_summary(rows, mtx_files):
    """Print a condensed table: one row per matrix, tools as columns."""
    # Focus on column/natural for the summary
    key_configs = [
        ("precoloring", "column", "natural"),
        ("precoloring", "column", "largest_first"),
        ("colpack",     "column", "natural"),
        ("colpack",     "column", "largest_first"),
        ("dsjm",        "column", "lfo"),
        ("dsjm",        "column", "slo"),
        ("julia",       "column", "natural"),
        ("julia",       "column", "largest_first"),
    ]

    # Index rows
    idx = {}
    for r in rows:
        k = (r["matrix"], r["tool"], r["side"], r["ordering"])
        idx[k] = r

    # Column headers
    headers = ["matrix", "rows", "cols", "nnz"]
    short = {
        ("precoloring","column","natural"):       "pre/nat",
        ("precoloring","column","largest_first"): "pre/lf",
        ("colpack",    "column","natural"):        "cp/nat",
        ("colpack",    "column","largest_first"):  "cp/lf",
        ("dsjm",       "column","lfo"):            "dsj/lfo",
        ("dsjm",       "column","slo"):            "dsj/slo",
        ("julia",      "column","natural"):        "jl/nat",
        ("julia",      "column","largest_first"):  "jl/lf",
    }

    col_names = headers + [f"{s}(#col,ms)" for s in short.values()]
    sep = " | "
    hdr = sep.join(f"{c:<20}" if c not in headers else f"{c:<18}" for c in col_names)
    print("\n" + "=" * min(len(hdr), 200))
    print("SUMMARY (column coloring, natural and largest-first orderings)")
    print("=" * min(len(hdr), 200))
    print(f"{'matrix':<26} {'rows':>7} {'cols':>7} {'nnz':>9}  " +
          "  ".join(f"{s:<16}" for s in short.values()))
    print("-" * min(len(hdr) + 40, 200))

    for mtx in mtx_files:
        bname = mtx.name
        ref = idx.get((bname, "precoloring", "column", "natural"), {})
        mr = ref.get("rows", "?")
        mc = ref.get("cols", "?")
        mn = ref.get("nnz",  "?")
        row_str = f"{bname:<26} {str(mr):>7} {str(mc):>7} {str(mn):>9}  "
        for (tool, side, ordering) in key_configs:
            r = idx.get((bname, tool, side, ordering), {})
            nc = r.get("num_colors", "-")
            tm = r.get("time_ms", "-")
            cell = f"{nc}/{tm}ms" if nc not in ("-", "ERROR") else "-"
            row_str += f"{cell:<16}  "
        print(row_str)


if __name__ == "__main__":
    main()
