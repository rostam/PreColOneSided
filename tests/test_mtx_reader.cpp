#include <catch2/catch_test_macros.hpp>

#include "mtx_reader.hpp"

// ── helpers ───────────────────────────────────────────────────────────────────

static const char* k_identity2 = R"(%%MatrixMarket matrix coordinate real general
2 2 2
1 1 1.0
2 2 1.0
)";

static const char* k_symmetric2 = R"(%%MatrixMarket matrix coordinate real symmetric
2 2 1
2 1 3.14
)";

static const char* k_pattern_general = R"(%%MatrixMarket matrix coordinate pattern general
3 4 5
1 1
1 3
2 2
2 4
3 1
)";

static const char* k_with_comments = R"(%%MatrixMarket matrix coordinate real general
% This is a comment
% Another comment
3 3 3
1 1 1.0
2 2 2.0
3 3 3.0
)";

// ── tests ─────────────────────────────────────────────────────────────────────

TEST_CASE("MTX: 2x2 identity (general)", "[mtx]") {
    const auto m = read_mtx_string(k_identity2);
    CHECK(m.rows == 2);
    CHECK(m.cols == 2);
    REQUIRE(m.row_indices.size() == 2);
    CHECK(m.row_indices[0] == 0);
    CHECK(m.col_indices[0] == 0);
    CHECK(m.row_indices[1] == 1);
    CHECK(m.col_indices[1] == 1);
    CHECK(m.symmetry == MTXSymmetry::GENERAL);
}

TEST_CASE("MTX: symmetric 2x2 expands off-diagonal", "[mtx]") {
    const auto m = read_mtx_string(k_symmetric2);
    // original entry (2,1) → (1,0) in 0-based
    // symmetric partner  (1,2) → (0,1) in 0-based
    REQUIRE(m.row_indices.size() == 2);
    CHECK(m.symmetry == MTXSymmetry::SYMMETRIC);

    // both (1,0) and (0,1) must be present
    bool has_10 = false, has_01 = false;
    for (std::size_t k = 0; k < m.row_indices.size(); ++k) {
        if (m.row_indices[k] == 1 && m.col_indices[k] == 0) has_10 = true;
        if (m.row_indices[k] == 0 && m.col_indices[k] == 1) has_01 = true;
    }
    CHECK(has_10);
    CHECK(has_01);
}

TEST_CASE("MTX: symmetric diagonal entry is NOT doubled", "[mtx]") {
    const char* diag = R"(%%MatrixMarket matrix coordinate real symmetric
3 3 2
1 1 5.0
2 2 3.0
)";
    const auto m = read_mtx_string(diag);
    // diagonal entries must not be duplicated
    CHECK(m.row_indices.size() == 2);
}

TEST_CASE("MTX: pattern general", "[mtx]") {
    const auto m = read_mtx_string(k_pattern_general);
    CHECK(m.rows == 3);
    CHECK(m.cols == 4);
    CHECK(m.row_indices.size() == 5);
}

TEST_CASE("MTX: comment lines are skipped", "[mtx]") {
    const auto m = read_mtx_string(k_with_comments);
    CHECK(m.rows == 3);
    CHECK(m.cols == 3);
    CHECK(m.row_indices.size() == 3);
}

TEST_CASE("MTX: indices are converted to 0-based", "[mtx]") {
    const char* mtx = R"(%%MatrixMarket matrix coordinate real general
5 6 1
3 4 9.9
)";
    const auto m = read_mtx_string(mtx);
    CHECK(m.row_indices[0] == 2);
    CHECK(m.col_indices[0] == 3);
}

TEST_CASE("MTX: missing banner throws", "[mtx]") {
    CHECK_THROWS_AS(read_mtx_string("not a mtx file\n1 1 1\n1 1 1.0\n"),
                    std::runtime_error);
}

TEST_CASE("MTX: wrong object type throws", "[mtx]") {
    CHECK_THROWS_AS(
        read_mtx_string("%%MatrixMarket vector coordinate real general\n1 1\n1 0.0\n"),
        std::runtime_error);
}

TEST_CASE("MTX: array format throws", "[mtx]") {
    CHECK_THROWS_AS(
        read_mtx_string("%%MatrixMarket matrix array real general\n2 2\n1.0\n2.0\n3.0\n4.0\n"),
        std::runtime_error);
}

TEST_CASE("MTX: empty file throws", "[mtx]") {
    CHECK_THROWS_AS(read_mtx_string(""), std::runtime_error);
}
