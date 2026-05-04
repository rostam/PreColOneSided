#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <numeric>

#include "bipartite_graph.hpp"
#include "coloring.hpp"
#include "mtx_reader.hpp"
#include "required_edges.hpp"

// ── small graph factories ─────────────────────────────────────────────────────

// K_{m,n}: every row connects to every column
static BipartiteGraph complete_bipartite(int m, int n) {
    std::vector<std::pair<int,int>> e;
    for (int r = 0; r < m; ++r)
        for (int c = 0; c < n; ++c)
            e.emplace_back(r, c);
    return BipartiteGraph(m, n, e);
}

// ── RequiredEdges unit tests ──────────────────────────────────────────────────

TEST_CASE("RequiredEdges: empty on construction", "[required]") {
    RequiredEdges req;
    CHECK(req.empty());
    CHECK(req.size() == 0);
    CHECK_FALSE(req.contains(0, 0));
}

TEST_CASE("RequiredEdges: add and contains", "[required]") {
    RequiredEdges req;
    req.add(2, 5);
    req.add(0, 0);
    CHECK(req.contains(2, 5));
    CHECK(req.contains(0, 0));
    CHECK_FALSE(req.contains(5, 2));  // order matters
    CHECK_FALSE(req.contains(1, 1));
    CHECK(req.size() == 2);
}

TEST_CASE("RequiredEdges: duplicate add does not grow size", "[required]") {
    RequiredEdges req;
    req.add(3, 7);
    req.add(3, 7);
    CHECK(req.size() == 1);
}

TEST_CASE("RequiredEdges: clear", "[required]") {
    RequiredEdges req;
    req.add(0, 0); req.add(1, 1);
    req.clear();
    CHECK(req.empty());
    CHECK_FALSE(req.contains(0, 0));
}

// ── diagonal_block_required tests ─────────────────────────────────────────────

TEST_CASE("diagonal_block_required: 4x4 identity, block=2", "[required]") {
    // 4×4 identity: edges (0,0),(1,1),(2,2),(3,3)
    BipartiteGraph g(4, 4, {{0,0},{1,1},{2,2},{3,3}});
    auto req = diagonal_block_required(g, 2);
    // Block 0: rows 0-1, cols 0-1  → (0,0) and (1,1) exist
    // Block 1: rows 2-3, cols 2-3  → (2,2) and (3,3) exist
    CHECK(req.size() == 4);
    CHECK(req.contains(0, 0));
    CHECK(req.contains(1, 1));
    CHECK(req.contains(2, 2));
    CHECK(req.contains(3, 3));
}

TEST_CASE("diagonal_block_required: cross-block edges not included", "[required]") {
    // Edge (0,2) crosses block boundary with block=2 → not required
    BipartiteGraph g(4, 4, {{0,0},{0,2},{2,2}});
    auto req = diagonal_block_required(g, 2);
    CHECK(req.contains(0, 0));
    CHECK(req.contains(2, 2));
    CHECK_FALSE(req.contains(0, 2));
}

TEST_CASE("diagonal_block_required: block larger than matrix → no required edges", "[required]") {
    BipartiteGraph g(3, 3, {{0,0},{1,1}});
    auto req = diagonal_block_required(g, 5);
    CHECK(req.empty());  // 5 > min(3,3) so no complete blocks
}

TEST_CASE("diagonal_block_required: rectangular matrix, block=2", "[required]") {
    // 4 rows × 6 cols, block=2: blocks use min(4,6)/2 = 2 blocks
    // Block 0: rows 0-1, cols 0-1
    // Block 1: rows 2-3, cols 2-3
    BipartiteGraph g(4, 6, {{0,0},{0,5},{1,1},{2,2},{3,3},{3,5}});
    auto req = diagonal_block_required(g, 2);
    CHECK(req.contains(0, 0));
    CHECK(req.contains(1, 1));
    CHECK(req.contains(2, 2));
    CHECK(req.contains(3, 3));
    CHECK_FALSE(req.contains(0, 5));  // col 5 outside block 0
    CHECK_FALSE(req.contains(3, 5));  // col 5 outside block 1
}

// ── greedy_color_restricted correctness ──────────────────────────────────────

TEST_CASE("Restricted: no required edges → all color 0 (no conflicts)", "[restricted]") {
    // All columns share rows but no edges are required → no conflicts at all
    auto g = complete_bipartite(3, 4);
    RequiredEdges req;  // empty
    auto r = greedy_color_restricted(g, ColoringSide::COLUMNS,
                                     OrderingStrategy::NATURAL, req);
    CHECK(r.num_colors == 1);
    for (int c : r.colors) CHECK(c == 0);
    CHECK(verify_restricted_coloring(g, r, req));
}

TEST_CASE("Restricted: all edges required → same result as regular coloring", "[restricted]") {
    auto g = complete_bipartite(3, 5);
    auto order = generate_ordering(g, ColoringSide::COLUMNS,
                                   OrderingStrategy::NATURAL);
    // Mark every edge required
    RequiredEdges req;
    for (int r = 0; r < g.num_rows(); ++r)
        for (int c : g.row_neighbors(r))
            req.add(r, c);

    auto reg = greedy_color(g, ColoringSide::COLUMNS, order);
    auto res = greedy_color_restricted(g, ColoringSide::COLUMNS, order, req);

    CHECK(res.num_colors == reg.num_colors);
    CHECK(res.colors == reg.colors);
    CHECK(verify_restricted_coloring(g, res, req));
}

TEST_CASE("Restricted: partial requirements reduce color count", "[restricted]") {
    // 3×3 all-ones matrix.
    // With regular coloring (natural): 3 colors (all columns conflict pairwise).
    // Required: only edge (row0, col0).
    // col0-col1 conflict via row0 (req edge present).
    // col0-col2 conflict via row0 (req edge present).
    // col1-col2: share row0 but neither (row0,col1) nor (row0,col2) is required;
    //            share row1,row2 — none required → NO conflict.
    // Expected restricted colors: [0, 1, 1] → 2 colors.
    auto g = complete_bipartite(3, 3);
    RequiredEdges req;
    req.add(0, 0);  // only one required edge

    auto r = greedy_color_restricted(g, ColoringSide::COLUMNS,
                                     OrderingStrategy::NATURAL, req);
    CHECK(r.num_colors == 2);
    CHECK(r.colors[0] != r.colors[1]);  // col0 and col1 conflict
    CHECK(r.colors[0] != r.colors[2]);  // col0 and col2 conflict
    CHECK(r.colors[1] == r.colors[2]);  // col1 and col2 do NOT conflict
    CHECK(verify_restricted_coloring(g, r, req));
}

TEST_CASE("Restricted: colors <= regular colors for same ordering (property)", "[restricted]") {
    // Build a moderately connected graph and verify the invariant holds
    // for all four ordering strategies.
    BipartiteGraph g(6, 8, {
        {0,0},{0,1},{0,4},{1,1},{1,2},{1,5},{2,2},{2,3},{2,6},
        {3,3},{3,4},{3,7},{4,0},{4,5},{5,1},{5,6}
    });
    RequiredEdges req;
    req.add(0,0); req.add(1,1); req.add(2,2); req.add(3,3);

    for (auto strat : {OrderingStrategy::NATURAL,
                       OrderingStrategy::RANDOM,
                       OrderingStrategy::LARGEST_DEGREE_FIRST,
                       OrderingStrategy::SMALLEST_DEGREE_FIRST}) {
        auto order = generate_ordering(g, ColoringSide::COLUMNS, strat, 0);
        auto reg = greedy_color(g, ColoringSide::COLUMNS, order);
        auto res = greedy_color_restricted(g, ColoringSide::COLUMNS, order, req);
        CHECK(res.num_colors <= reg.num_colors);
        CHECK(verify_restricted_coloring(g, res, req));
    }
}

TEST_CASE("Restricted: row-side coloring", "[restricted]") {
    // Mirror of the partial-requirements test but for rows.
    // 3×3 all-ones; required: only (row0, col0).
    // Rows 1 and 2 share col0 but the edge (row1,col0) and (row2,col0) are not required;
    // they also share col1 and col2 (none required) → rows 1,2 do NOT conflict.
    auto g = complete_bipartite(3, 3);
    RequiredEdges req;
    req.add(0, 0);

    auto r = greedy_color_restricted(g, ColoringSide::ROWS,
                                     OrderingStrategy::NATURAL, req);
    CHECK(r.num_colors == 2);
    CHECK(r.colors[0] != r.colors[1]);
    CHECK(r.colors[0] != r.colors[2]);
    CHECK(r.colors[1] == r.colors[2]);
    CHECK(verify_restricted_coloring(g, r, req));
}

// ── verify_restricted_coloring ────────────────────────────────────────────────

TEST_CASE("Verify restricted: valid coloring passes", "[restricted]") {
    auto g = complete_bipartite(2, 3);
    RequiredEdges req;
    req.add(0, 0);

    auto r = greedy_color_restricted(g, ColoringSide::COLUMNS,
                                     OrderingStrategy::NATURAL, req);
    CHECK(verify_restricted_coloring(g, r, req));
}

TEST_CASE("Verify restricted: detects same color on conflicting pair", "[restricted]") {
    // star_row: 1 row × 3 cols, all edges. Required: (row0,col0).
    // col0 conflicts with col1 and col2 (via (row0,col0) which is required).
    BipartiteGraph g(1, 3, {{0,0},{0,1},{0,2}});
    RequiredEdges req;
    req.add(0, 0);

    ColoringResult bad;
    bad.side = ColoringSide::COLUMNS;
    bad.colors = {0, 0, 0};  // col0 same color as col1,col2 → invalid
    bad.num_colors = 1;
    CHECK_FALSE(verify_restricted_coloring(g, bad, req));
}

TEST_CASE("Verify restricted: allows same color when no required edge on path", "[restricted]") {
    // 1 row × 3 cols, no required edges.
    BipartiteGraph g(1, 3, {{0,0},{0,1},{0,2}});
    RequiredEdges req;  // empty

    ColoringResult ok;
    ok.side = ColoringSide::COLUMNS;
    ok.colors = {0, 0, 0};  // all same color — fine because no required edges
    ok.num_colors = 1;
    CHECK(verify_restricted_coloring(g, ok, req));
}

TEST_CASE("Verify restricted: uncolored vertex returns false", "[restricted]") {
    BipartiteGraph g(1, 2, {{0,0},{0,1}});
    RequiredEdges req;
    ColoringResult bad;
    bad.side = ColoringSide::COLUMNS;
    bad.colors = {0, -1};
    bad.num_colors = 1;
    CHECK_FALSE(verify_restricted_coloring(g, bad, req));
}

// ── diagonal block integration ────────────────────────────────────────────────

TEST_CASE("Restricted: diagonal blocks on identity graph → 1 color", "[restricted]") {
    // 8×8 identity: no two columns share a row → regular coloring = 1 color.
    // Restricted coloring with any required edges also = 1 color.
    BipartiteGraph g(8, 8, {{0,0},{1,1},{2,2},{3,3},{4,4},{5,5},{6,6},{7,7}});
    auto req = diagonal_block_required(g, 2);
    auto r = greedy_color_restricted(g, ColoringSide::COLUMNS,
                                     OrderingStrategy::NATURAL, req);
    CHECK(r.num_colors == 1);
    CHECK(verify_restricted_coloring(g, r, req));
}

TEST_CASE("Restricted: real MTX file, restricted <= regular for all orderings", "[restricted][integration]") {
    auto mat = read_mtx("FloridaSparseMatrixCollection/bcsstk01.mtx");
    BipartiteGraph g(mat);
    auto req = diagonal_block_required(g, 4);

    for (auto strat : {OrderingStrategy::NATURAL,
                       OrderingStrategy::LARGEST_DEGREE_FIRST,
                       OrderingStrategy::SMALLEST_DEGREE_FIRST}) {
        auto order = generate_ordering(g, ColoringSide::COLUMNS, strat);
        auto reg = greedy_color(g, ColoringSide::COLUMNS, order);
        auto res = greedy_color_restricted(g, ColoringSide::COLUMNS, order, req);
        CHECK(res.num_colors <= reg.num_colors);
        CHECK(verify_restricted_coloring(g, res, req));
    }
}

TEST_CASE("Restricted: real MTX file, block=8, verify passes", "[restricted][integration]") {
    auto mat = read_mtx("FloridaSparseMatrixCollection/mymatrix.mtx");
    BipartiteGraph g(mat);
    auto req = diagonal_block_required(g, 2);
    auto r = greedy_color_restricted(g, ColoringSide::COLUMNS,
                                     OrderingStrategy::NATURAL, req);
    CHECK(verify_restricted_coloring(g, r, req));
    // Restricted must not exceed regular
    auto reg = greedy_color(g, ColoringSide::COLUMNS);
    CHECK(r.num_colors <= reg.num_colors);
}

TEST_CASE("Restricted: larger MTX, multiple block sizes", "[restricted][integration]") {
    auto mat = read_mtx("FloridaSparseMatrixCollection/bcsstm27.mtx");
    BipartiteGraph g(mat);
    auto reg = greedy_color(g, ColoringSide::COLUMNS);

    for (int bs : {2, 4, 8, 16, 32}) {
        auto req = diagonal_block_required(g, bs);
        auto res = greedy_color_restricted(g, ColoringSide::COLUMNS,
                                           OrderingStrategy::NATURAL, req);
        CHECK(res.num_colors <= reg.num_colors);
        CHECK(verify_restricted_coloring(g, res, req));
    }
}
