#include "bipartite_graph.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <stdexcept>

// ── private helper ────────────────────────────────────────────────────────────

void BipartiteGraph::build(const std::vector<int>& row_idx,
                            const std::vector<int>& col_idx) {
    const int n = static_cast<int>(row_idx.size());
    for (int k = 0; k < n; ++k) {
        const int r = row_idx[k];
        const int c = col_idx[k];
        if (r < 0 || r >= m_num_rows || c < 0 || c >= m_num_cols)
            throw std::runtime_error(
                std::format("BipartiteGraph: edge ({},{}) out of range {}x{}",
                            r, c, m_num_rows, m_num_cols));
        m_row_adj[r].push_back(c);
        m_col_adj[c].push_back(r);
    }

    // Sort and deduplicate adjacency lists (MTX files may contain duplicates).
    auto dedup = [](std::vector<int>& v) {
        std::ranges::sort(v);
        v.erase(std::ranges::unique(v).begin(), v.end());
    };
    for (auto& adj : m_row_adj) dedup(adj);
    for (auto& adj : m_col_adj) dedup(adj);
}

// ── constructors ──────────────────────────────────────────────────────────────

BipartiteGraph::BipartiteGraph(const MTXMatrix& mat)
    : m_num_rows(mat.rows),
      m_num_cols(mat.cols),
      m_row_adj(mat.rows),
      m_col_adj(mat.cols) {
    build(mat.row_indices, mat.col_indices);
}

BipartiteGraph::BipartiteGraph(int num_rows, int num_cols,
                               const std::vector<std::pair<int, int>>& edges)
    : m_num_rows(num_rows),
      m_num_cols(num_cols),
      m_row_adj(num_rows),
      m_col_adj(num_cols) {
    std::vector<int> rs, cs;
    rs.reserve(edges.size());
    cs.reserve(edges.size());
    for (auto [r, c] : edges) { rs.push_back(r); cs.push_back(c); }
    build(rs, cs);
}

// ── accessors ─────────────────────────────────────────────────────────────────

std::span<const int> BipartiteGraph::row_neighbors(int row) const {
    return m_row_adj[row];
}

std::span<const int> BipartiteGraph::col_neighbors(int col) const {
    return m_col_adj[col];
}

int BipartiteGraph::row_degree(int row) const {
    return static_cast<int>(m_row_adj[row].size());
}

int BipartiteGraph::col_degree(int col) const {
    return static_cast<int>(m_col_adj[col].size());
}
