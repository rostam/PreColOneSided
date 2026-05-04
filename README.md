# PreColOneSided

Greedy **one-sided distance-2 coloring** of bipartite graphs derived from sparse matrices in [Matrix Market (MTX)](https://math.nist.gov/MatrixMarket/formats.html) format.

## Background

Given a sparse matrix **A** of size *m × n*, a bipartite graph **B = (R ∪ C, E)** is constructed where:

- **R** = {row vertices 0 … m−1}
- **C** = {column vertices 0 … n−1}
- edge *(r, c) ∈ E* iff *A[r][c] ≠ 0*

**One-sided coloring** colors only one partition (rows *or* columns) such that no two vertices on that side share the same color if they are at **distance 2** through the bipartite graph — i.e., if they have a common neighbour on the opposite side.

This is equivalent to the partial distance-2 coloring used in efficient sparse Jacobian and Hessian computation.

### Greedy algorithm

For a chosen side (say columns), vertices are visited in a configurable order. Each vertex is assigned the **minimum non-negative integer color** not already used by any of its distance-2 neighbours that have been colored earlier.

## Build

Requires **CMake ≥ 3.20** and a compiler with **C++23** support (GCC 13+ or Clang 16+). An internet connection is needed on the first build to fetch [Catch2 v3](https://github.com/catchorg/Catch2) for the tests.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Usage

```
./build/precol <matrix.mtx> [options]

Options:
  --side    rows|columns     Which side to color (default: columns)
  --order   natural|random|largest|smallest
                             Vertex ordering strategy (default: natural)
  --seed    <uint>           RNG seed for random ordering (default: 42)
  --verify                  Verify the coloring after computing it
```

### Ordering strategies

| Flag | Description |
|------|-------------|
| `natural` | Vertices in index order 0, 1, 2, … |
| `random` | Random permutation (use `--seed` to reproduce) |
| `largest` | Largest bipartite degree first |
| `smallest` | Smallest bipartite degree first |

### Examples

```bash
# Color columns of a symmetric matrix, verify the result
./build/precol FloridaSparseMatrixCollection/bcsstk01.mtx --verify

# Color rows using largest-degree-first ordering
./build/precol FloridaSparseMatrixCollection/ash608.mtx --side rows --order largest --verify

# Reproducible random ordering
./build/precol FloridaSparseMatrixCollection/mac_econ_fwd500.mtx --order random --seed 123 --verify
```

Sample output:

```
Matrix: bcsstk01.mtx  (48 rows × 48 cols, 400 structural non-zeros)
Side: columns  |  Order: natural  |  Colors used: 15
Load: 0.08 ms  |  Coloring: 0.01 ms
Verification: PASSED
```

## Run tests

```bash
cd build && ctest --output-on-failure
# or run directly for full details:
./build/tests
```

37 test cases cover the MTX parser, bipartite graph construction, ordering strategies, coloring correctness, the verifier, and integration tests on real matrices.

## Project structure

```
src/
  mtx_reader.hpp/cpp       MTX parser (real/integer/complex/pattern, all symmetry types)
  bipartite_graph.hpp/cpp  Bipartite graph with sorted, deduplicated adjacency lists
  coloring.hpp/cpp         Greedy coloring, ordering strategies, verifier
  main.cpp                 CLI entry point
tests/
  test_mtx_reader.cpp
  test_bipartite_graph.cpp
  test_coloring.cpp
```
