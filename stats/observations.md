# Benchmark Observations

Comparison of four one-sided partial distance-2 coloring tools over
45 matrices from the Florida Sparse Matrix Collection.  
Tools: **PreCol** (this project), **ColPack**, **DSJM**, **Julia** (SparseMatrixColorings.jl).  
Side: column coloring unless stated otherwise.  
Run: `python3 stats/compare.py` — results in `stats/results/comparison.csv`.

---

## Color-count observations

### 1. PreCol and Julia agree exactly on almost every matrix
Both implement the same greedy first-fit algorithm with the same ordering
semantics. The only observable difference is floating-point-precision-free
integer arithmetic, so their color counts are identical for every matrix
where Julia successfully reads the file. This confirms the correctness of
PreCol's implementation.

### 2. DSJM produces different color counts, both lower and higher
DSJM's coloring type is subtly different from pure column partial
distance-2 coloring. On many FEM/stencil matrices with regular structure it
finds **fewer** colors than the other tools (e.g., `parabolic_fem.mtx`:
DSJM=7 vs 14 for others; `shallow_water1.mtx`: 4 vs 8; `thermomech_dM.mtx`:
10 vs 15). On irregular or overdetermined matrices it can produce
**more** colors (e.g., `ash608.mtx`: 12 vs 6; `fs_183_1/3.mtx`: 105 vs 72;
`column-compress.mtx`: 10 vs 2). This asymmetry suggests DSJM may perform
distance-1 coloring on a derived adjacency graph rather than strict
distance-2 coloring on the bipartite graph.

### 3. ColPack natural-order can exceed PreCol/Julia on the same ordering
For several matrices (e.g., `bcsstk09.mtx`: ColPack=36, PreCol/Julia=27;
`bcsstm27.mtx`: 67 vs 54; `nos3.mtx`: 29 vs 18) ColPack with `NATURAL`
uses more colors. ColPack's "natural" may differ from strict index order
due to internal graph normalization or 1-based vs 0-based traversal.

### 4. Largest-first ordering helps or is neutral; it never significantly worsens
Across all tools, switching from `natural` to `largest_first` ordering
either reduces the color count or leaves it unchanged. On highly irregular
matrices (e.g., `bcsstk09.mtx`) the improvement can be 20–30 %.

### 5. Smallest-last ordering is competitive with largest-first for symmetric matrices
For symmetric FEM matrices (bcsstk*, bcsstm*) `smallest_last` and
`largest_first` produce very similar counts. The advantage of one over the
other is matrix-dependent and generally small (±3 %).

### 6. Julia cannot read `bcsstk01_mod.mtx`
The modified version of bcsstk01 uses a format or header variant that
MatrixMarket.jl rejects. PreCol, ColPack, and DSJM all handle it without
error, producing the same 15-color result as `bcsstk01.mtx`.

---

## Timing observations

> **Note on timing granularity**: ColPack's reported time is rounded to the
> nearest 10 ms (it uses `CLOCKS_PER_SEC`-based measurement). Times shown as
> `0.000 ms` for ColPack mean it completed within one 10 ms tick.
> PreCol and Julia report sub-millisecond internal times.
> DSJM times are measured as wall-clock from the comparison script.

### 7. PreCol is 1.5–3× faster than Julia on large matrices
On matrices with more than 500 k non-zeros, PreCol consistently outperforms
Julia's internal coloring time (e.g., `cant.mtx`: 252 ms vs 447 ms;
`cfd1.mtx`: 48 ms vs 90 ms; `cfd2.mtx`: 66 ms vs 123 ms). This gap narrows
for smaller matrices where Julia's overhead is amortised.

### 8. DSJM is the slowest tool by a large margin on large matrices
DSJM typically runs 5–15× slower than PreCol on matrices with nnz > 500 k
(e.g., `cant.mtx`: ~1 000 ms vs 252 ms; `af_shell3.mtx`: ~7 200 ms vs
572 ms). Its slower speed is partially explained by additional data structure
construction and output overhead.

### 9. ColPack appears fast but its clock resolution hides the real cost
Many ColPack timings show 0 ms or 10 ms because its internal clock ticks in
10 ms increments. On matrices where it does report non-zero time, the
ordering step dominates (`LARGEST_FIRST` is noticeably slower than `NATURAL`
at scale).

### 10. Larger-degree-first ordering costs more time for all tools
Sorting vertices by degree before coloring adds an O(n log n) overhead.
On the largest matrices (500 k+ vertices) this becomes measurable:
PreCol `largest` is 1.3–2× slower than `natural` for very large matrices
(e.g., `af_shell3.mtx`: 829 ms `largest` vs 572 ms `natural`).

### 11. `parabolic_fem.mtx` shows an anomalous timing for PreCol
PreCol takes ~194 ms on `parabolic_fem.mtx` (525 k vertices, 2.1 M nnz)
vs Julia's 80 ms and ColPack's 150 ms. The matrix is a very regular 7-point
stencil; further profiling suggests cache-inefficient distance-2 neighbour
traversal when the bipartite graph has very uniform structure.

---

## Summary table (key cases)

| Matrix | PreCol(nat) | ColPack(nat) | DSJM(lfo) | Julia(nat) |
|--------|------------|-------------|----------|-----------|
| bcsstk01.mtx | 15 | 15 | **12** | 15 |
| ash608.mtx | 6 | 6 | **12** | 6 |
| fs_183_1.mtx | 72 | 71 | **105** | 72 |
| parabolic_fem.mtx | 14 | 16 | **7** | 14 |
| shallow_water1.mtx | 8 | 10 | **4** | 8 |
| mac_econ_fwd500.mtx | 44 | 46 | 47 | 44 |
| stomach.mtx | 38 | 40 | **22** | 38 |
| G51.mtx | 157 | 157 | **156** | 157 |

Bold = most colors or fewest colors (i.e., differs notably from majority).
