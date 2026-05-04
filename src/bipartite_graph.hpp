#pragma once

#include <span>
#include <vector>

#include "mtx_reader.hpp"

// Bipartite graph B = (R ∪ C, E) derived from a sparse matrix A.
// R = {row vertices 0 … m-1}, C = {column vertices 0 … n-1}.
// Edge (r, c) ∈ E  ⟺  A[r][c] ≠ 0.
class BipartiteGraph {
public:
    // Build from a parsed MTX matrix.
    explicit BipartiteGraph(const MTXMatrix& mat);

    // Build directly from an edge list (0-based indices). Useful in tests.
    BipartiteGraph(int num_rows, int num_cols,
                   const std::vector<std::pair<int, int>>& edges);

    int num_rows() const noexcept { return m_num_rows; }
    int num_cols() const noexcept { return m_num_cols; }

    // Column neighbors of a row vertex (i.e. columns with A[row][?] ≠ 0).
    std::span<const int> row_neighbors(int row) const;

    // Row neighbors of a column vertex (i.e. rows with A[?][col] ≠ 0).
    std::span<const int> col_neighbors(int col) const;

    int row_degree(int row) const;
    int col_degree(int col) const;

private:
    void build(const std::vector<int>& row_idx, const std::vector<int>& col_idx);

    int m_num_rows;
    int m_num_cols;
    std::vector<std::vector<int>> m_row_adj; // row  -> sorted list of col indices
    std::vector<std::vector<int>> m_col_adj; // col  -> sorted list of row indices
};
