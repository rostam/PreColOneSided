#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include "bipartite_graph.hpp"
#include "mtx_reader.hpp"

// ── helpers ───────────────────────────────────────────────────────────────────

static BipartiteGraph make_identity(int n) {
    // nxn identity: edge (i,i) for i in 0..n-1
    std::vector<std::pair<int,int>> edges;
    for (int i = 0; i < n; ++i) edges.emplace_back(i, i);
    return BipartiteGraph(n, n, edges);
}

static BipartiteGraph make_complete_bipartite(int m, int n) {
    // K_{m,n}: every row connects to every column
    std::vector<std::pair<int,int>> edges;
    for (int r = 0; r < m; ++r)
        for (int c = 0; c < n; ++c)
            edges.emplace_back(r, c);
    return BipartiteGraph(m, n, edges);
}

// ── tests ─────────────────────────────────────────────────────────────────────

TEST_CASE("BipartiteGraph: identity matrix", "[graph]") {
    auto g = make_identity(4);
    CHECK(g.num_rows() == 4);
    CHECK(g.num_cols() == 4);
    // row i has exactly one column neighbour: col i
    for (int i = 0; i < 4; ++i) {
        REQUIRE(g.row_degree(i) == 1);
        CHECK(g.row_neighbors(i)[0] == i);
        REQUIRE(g.col_degree(i) == 1);
        CHECK(g.col_neighbors(i)[0] == i);
    }
}

TEST_CASE("BipartiteGraph: complete bipartite K_{2,3}", "[graph]") {
    auto g = make_complete_bipartite(2, 3);
    CHECK(g.num_rows() == 2);
    CHECK(g.num_cols() == 3);
    for (int r = 0; r < 2; ++r) CHECK(g.row_degree(r) == 3);
    for (int c = 0; c < 3; ++c) CHECK(g.col_degree(c) == 2);
}

TEST_CASE("BipartiteGraph: neighbour lists are sorted", "[graph]") {
    // Insert edges in reverse order; lists must still be sorted.
    std::vector<std::pair<int,int>> edges = {{2,0},{1,0},{0,0}};
    BipartiteGraph g(3, 1, edges);
    auto nb = g.col_neighbors(0);
    CHECK(std::ranges::is_sorted(nb));
}

TEST_CASE("BipartiteGraph: duplicate edges are deduplicated", "[graph]") {
    std::vector<std::pair<int,int>> edges = {{0,0},{0,0},{0,1},{0,1}};
    BipartiteGraph g(1, 2, edges);
    CHECK(g.row_degree(0) == 2);
    CHECK(g.col_degree(0) == 1);
    CHECK(g.col_degree(1) == 1);
}

TEST_CASE("BipartiteGraph: construction from MTXMatrix", "[graph]") {
    const char* mtx = R"(%%MatrixMarket matrix coordinate real general
3 4 5
1 1 1.0
1 3 1.0
2 2 1.0
2 4 1.0
3 1 1.0
)";
    auto mat = read_mtx_string(mtx);
    BipartiteGraph g(mat);
    CHECK(g.num_rows() == 3);
    CHECK(g.num_cols() == 4);
    CHECK(g.row_degree(0) == 2); // row 0 → cols {0, 2}
    CHECK(g.row_degree(1) == 2); // row 1 → cols {1, 3}
    CHECK(g.row_degree(2) == 1); // row 2 → col  {0}
    CHECK(g.col_degree(0) == 2); // col 0 ← rows {0, 2}
    CHECK(g.col_degree(1) == 1); // col 1 ← row  {1}
}

TEST_CASE("BipartiteGraph: symmetric MTX expands to full graph", "[graph]") {
    const char* mtx = R"(%%MatrixMarket matrix coordinate real symmetric
3 3 3
2 1 1.0
3 1 1.0
3 2 1.0
)";
    auto mat = read_mtx_string(mtx);
    BipartiteGraph g(mat);
    // After expansion each row and col should have degree 2
    CHECK(g.row_degree(0) == 2); // row 0: cols {1,2}
    CHECK(g.row_degree(1) == 2); // row 1: cols {0,2}
    CHECK(g.row_degree(2) == 2); // row 2: cols {0,1}
}

TEST_CASE("BipartiteGraph: single-entry matrix", "[graph]") {
    BipartiteGraph g(5, 7, {{2, 4}});
    CHECK(g.row_degree(2) == 1);
    CHECK(g.col_degree(4) == 1);
    for (int r = 0; r < 5; ++r) if (r != 2) CHECK(g.row_degree(r) == 0);
    for (int c = 0; c < 7; ++c) if (c != 4) CHECK(g.col_degree(c) == 0);
}
