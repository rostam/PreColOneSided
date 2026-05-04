#pragma once

#include <cstdint>
#include <unordered_set>

#include "bipartite_graph.hpp"

// Sparse set of edges (row, col) marked as required for restricted coloring.
// Lookup is O(1) amortised via an unordered_set on a packed 64-bit key.
class RequiredEdges {
public:
    void add(int row, int col) { m_set.insert(key(row, col)); }

    [[nodiscard]] bool contains(int row, int col) const noexcept {
        return m_set.contains(key(row, col));
    }

    [[nodiscard]] bool   empty() const noexcept { return m_set.empty(); }
    [[nodiscard]] size_t size()  const noexcept { return m_set.size();  }
    void clear() { m_set.clear(); }

private:
    static constexpr std::uint64_t key(int r, int c) noexcept {
        return (std::uint64_t)(std::uint32_t)r << 32 | (std::uint32_t)c;
    }
    std::unordered_set<std::uint64_t> m_set;
};

// Mark every existing edge (nonzero) whose row and column both fall inside
// the same axis-aligned diagonal block of size block_size × block_size as
// required.  Block k covers rows and columns [k*bs, (k+1)*bs).
RequiredEdges diagonal_block_required(const BipartiteGraph& g, int block_size);
