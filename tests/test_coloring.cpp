#include <algorithm>
#include <numeric>
#include <catch2/catch_test_macros.hpp>

#include "bipartite_graph.hpp"
#include "coloring.hpp"

// ── small graph factories ─────────────────────────────────────────────────────

// nxn identity: each col has exactly one row neighbour → 0 distance-2 neighbours
static BipartiteGraph identity(int n) {
    std::vector<std::pair<int,int>> e;
    for (int i = 0; i < n; ++i) e.emplace_back(i, i);
    return BipartiteGraph(n, n, e);
}

// K_{m,n}: every row connects to every column
static BipartiteGraph complete_bipartite(int m, int n) {
    std::vector<std::pair<int,int>> e;
    for (int r = 0; r < m; ++r)
        for (int c = 0; c < n; ++c)
            e.emplace_back(r, c);
    return BipartiteGraph(m, n, e);
}

// "Column-sharing" graph: cols 0..n-1 all share exactly one row (row 0)
static BipartiteGraph star_row(int n_cols) {
    std::vector<std::pair<int,int>> e;
    for (int c = 0; c < n_cols; ++c) e.emplace_back(0, c);
    return BipartiteGraph(1, n_cols, e);
}

// Path of columns via shared rows: col0 - row0 - col1 - row1 - col2 - …
// Cols at even distance share no common row; adjacent pairs share one.
static BipartiteGraph col_path(int n_cols) {
    // edges: (r, r) and (r, r+1) for r in 0 … n_cols-2
    // number of rows = n_cols - 1
    std::vector<std::pair<int,int>> e;
    for (int r = 0; r < n_cols - 1; ++r) {
        e.emplace_back(r, r);
        e.emplace_back(r, r + 1);
    }
    return BipartiteGraph(n_cols - 1, n_cols, e);
}

// ── ordering tests ────────────────────────────────────────────────────────────

TEST_CASE("Ordering: natural is 0..n-1", "[ordering]") {
    auto g = identity(5);
    auto ord = generate_ordering(g, ColoringSide::COLUMNS, OrderingStrategy::NATURAL, 0);
    std::vector<int> expected(5);
    std::iota(expected.begin(), expected.end(), 0);
    CHECK(ord == expected);
}

TEST_CASE("Ordering: random is a permutation", "[ordering]") {
    auto g = identity(10);
    auto ord = generate_ordering(g, ColoringSide::COLUMNS, OrderingStrategy::RANDOM, 7);
    REQUIRE(ord.size() == 10);
    std::vector<int> sorted = ord;
    std::ranges::sort(sorted);
    for (int i = 0; i < 10; ++i) CHECK(sorted[i] == i);
}

TEST_CASE("Ordering: different seeds give different orders", "[ordering]") {
    auto g = identity(20);
    auto a = generate_ordering(g, ColoringSide::COLUMNS, OrderingStrategy::RANDOM, 1);
    auto b = generate_ordering(g, ColoringSide::COLUMNS, OrderingStrategy::RANDOM, 2);
    CHECK(a != b);
}

TEST_CASE("Ordering: largest-degree-first is non-increasing", "[ordering]") {
    auto g = complete_bipartite(3, 5);
    auto ord = generate_ordering(g, ColoringSide::COLUMNS,
                                 OrderingStrategy::LARGEST_DEGREE_FIRST, 0);
    for (std::size_t i = 1; i < ord.size(); ++i)
        CHECK(g.col_degree(ord[i-1]) >= g.col_degree(ord[i]));
}

TEST_CASE("Ordering: smallest-degree-first is non-decreasing", "[ordering]") {
    auto g = complete_bipartite(3, 5);
    auto ord = generate_ordering(g, ColoringSide::COLUMNS,
                                 OrderingStrategy::SMALLEST_DEGREE_FIRST, 0);
    for (std::size_t i = 1; i < ord.size(); ++i)
        CHECK(g.col_degree(ord[i-1]) <= g.col_degree(ord[i]));
}

TEST_CASE("Ordering: row side has correct length", "[ordering]") {
    auto g = complete_bipartite(4, 7);
    auto ord = generate_ordering(g, ColoringSide::ROWS, OrderingStrategy::NATURAL, 0);
    CHECK(ord.size() == 4);
}

// ── coloring correctness ──────────────────────────────────────────────────────

TEST_CASE("Coloring: identity needs exactly 1 color", "[coloring]") {
    auto g = identity(8);
    auto r = greedy_color(g, ColoringSide::COLUMNS);
    // No two column vertices share a row → all get color 0.
    CHECK(r.num_colors == 1);
    for (int c : r.colors) CHECK(c == 0);
    CHECK(verify_coloring(g, r));
}

TEST_CASE("Coloring: star (all cols share one row) needs n colors", "[coloring]") {
    const int n = 5;
    auto g = star_row(n);
    auto r = greedy_color(g, ColoringSide::COLUMNS);
    CHECK(r.num_colors == n);
    CHECK(verify_coloring(g, r));
}

TEST_CASE("Coloring: K_{2,2} needs 2 colors", "[coloring]") {
    // Both columns share both rows → they are distance-2 neighbours → need 2 colors
    auto g = complete_bipartite(2, 2);
    auto r = greedy_color(g, ColoringSide::COLUMNS);
    CHECK(r.num_colors == 2);
    CHECK(verify_coloring(g, r));
}

TEST_CASE("Coloring: K_{m,n} columns need m colors (all cols fully share all rows)", "[coloring]") {
    // All n cols are mutual distance-2 neighbours → chromatic number = n.
    const int m = 3, n = 4;
    auto g = complete_bipartite(m, n);
    auto r = greedy_color(g, ColoringSide::COLUMNS);
    CHECK(r.num_colors == n);
    CHECK(verify_coloring(g, r));
}

TEST_CASE("Coloring: col-path alternates two colors", "[coloring]") {
    // Adjacent cols in the path conflict; non-adjacent do not.
    // Chromatic number of a path = 2 (for n≥2).
    auto g = col_path(6);
    auto r = greedy_color(g, ColoringSide::COLUMNS);
    CHECK(r.num_colors == 2);
    CHECK(verify_coloring(g, r));
}

TEST_CASE("Coloring: row side also works correctly", "[coloring]") {
    // star_row: one row connects to all cols, so row side has 1 vertex → 1 color.
    auto g = star_row(5);
    auto r = greedy_color(g, ColoringSide::ROWS);
    CHECK(r.num_colors == 1);
    CHECK(verify_coloring(g, r));
}

TEST_CASE("Coloring: verify detects invalid coloring", "[coloring]") {
    auto g = star_row(3);
    // Manually assign same color to all three columns that share row 0.
    ColoringResult bad;
    bad.side = ColoringSide::COLUMNS;
    bad.colors = {0, 0, 0};
    bad.num_colors = 1;
    CHECK_FALSE(verify_coloring(g, bad));
}

TEST_CASE("Coloring: all ordering strategies give valid colorings", "[coloring]") {
    auto g = complete_bipartite(4, 6);
    for (auto strat : {OrderingStrategy::NATURAL,
                       OrderingStrategy::RANDOM,
                       OrderingStrategy::LARGEST_DEGREE_FIRST,
                       OrderingStrategy::SMALLEST_DEGREE_FIRST}) {
        auto r = greedy_color(g, ColoringSide::COLUMNS, strat, 123);
        CHECK(verify_coloring(g, r));
    }
}

TEST_CASE("Coloring: same seed → same result, different seed → possibly different", "[coloring]") {
    auto g = complete_bipartite(3, 8);
    auto r1 = greedy_color(g, ColoringSide::COLUMNS, OrderingStrategy::RANDOM, 42);
    auto r2 = greedy_color(g, ColoringSide::COLUMNS, OrderingStrategy::RANDOM, 42);
    CHECK(r1.colors == r2.colors);
}

TEST_CASE("Coloring: single vertex graph", "[coloring]") {
    BipartiteGraph g(1, 1, {{0, 0}});
    auto r = greedy_color(g, ColoringSide::COLUMNS);
    REQUIRE(r.colors.size() == 1);
    CHECK(r.colors[0] == 0);
    CHECK(r.num_colors == 1);
    CHECK(verify_coloring(g, r));
}

TEST_CASE("Coloring: empty graph (no edges)", "[coloring]") {
    BipartiteGraph g(3, 4, {});
    auto r = greedy_color(g, ColoringSide::COLUMNS);
    CHECK(r.num_colors == 1); // every vertex gets color 0
    for (int c : r.colors) CHECK(c == 0);
    CHECK(verify_coloring(g, r));
}

TEST_CASE("Coloring: from real MTX file (bcsstk01)", "[coloring][integration]") {
    auto mat = read_mtx("FloridaSparseMatrixCollection/bcsstk01.mtx");
    BipartiteGraph g(mat);
    // symmetric 48×48 → row and column sides are equivalent
    auto r_col = greedy_color(g, ColoringSide::COLUMNS);
    auto r_row = greedy_color(g, ColoringSide::ROWS);
    CHECK(verify_coloring(g, r_col));
    CHECK(verify_coloring(g, r_row));
    // Sanity: must use at least 1 color
    CHECK(r_col.num_colors >= 1);
}

TEST_CASE("Coloring: from real MTX file (ash608, rectangular)", "[coloring][integration]") {
    auto mat = read_mtx("FloridaSparseMatrixCollection/ash608.mtx");
    BipartiteGraph g(mat);
    auto r = greedy_color(g, ColoringSide::COLUMNS, OrderingStrategy::LARGEST_DEGREE_FIRST);
    CHECK(verify_coloring(g, r));
    CHECK(r.num_colors >= 1);
}

TEST_CASE("Coloring: from real MTX file (mymatrix, 3x6 general)", "[coloring][integration]") {
    auto mat = read_mtx("FloridaSparseMatrixCollection/mymatrix.mtx");
    BipartiteGraph g(mat);
    // 3 rows × 6 cols with 8 nonzeros
    CHECK(g.num_rows() == 3);
    CHECK(g.num_cols() == 6);
    auto r = greedy_color(g, ColoringSide::COLUMNS);
    CHECK(verify_coloring(g, r));
}
