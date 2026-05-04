#include "coloring.hpp"

#include <algorithm>
#include <numeric>
#include <random>
#include <ranges>
#include <stdexcept>

// ── ordering ──────────────────────────────────────────────────────────────────

std::vector<int> generate_ordering(const BipartiteGraph& g,
                                   ColoringSide side,
                                   OrderingStrategy strategy,
                                   unsigned seed) {
    const int n = (side == ColoringSide::COLUMNS) ? g.num_cols() : g.num_rows();

    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);

    auto degree = [&](int v) {
        return (side == ColoringSide::COLUMNS) ? g.col_degree(v) : g.row_degree(v);
    };

    switch (strategy) {
        case OrderingStrategy::NATURAL:
            break; // already 0 … n-1

        case OrderingStrategy::RANDOM: {
            std::mt19937 rng(seed);
            std::ranges::shuffle(order, rng);
            break;
        }

        case OrderingStrategy::LARGEST_DEGREE_FIRST:
            std::ranges::sort(order, [&](int a, int b) {
                return degree(a) > degree(b);
            });
            break;

        case OrderingStrategy::SMALLEST_DEGREE_FIRST:
            std::ranges::sort(order, [&](int a, int b) {
                return degree(a) < degree(b);
            });
            break;
    }
    return order;
}

// ── greedy coloring ───────────────────────────────────────────────────────────

ColoringResult greedy_color(const BipartiteGraph& g,
                            ColoringSide side,
                            std::span<const int> order) {
    const int n = (side == ColoringSide::COLUMNS) ? g.num_cols() : g.num_rows();
    const int sz = static_cast<int>(order.size());
    if (sz != n)
        throw std::invalid_argument("order length does not match vertex count");

    // colors[v] == -1  means v is not yet colored.
    ColoringResult result;
    result.side = side;
    result.colors.assign(n, -1);

    // forbidden[c] == current_vertex_id  means color c is taken by a
    // distance-2 neighbour of the vertex currently being processed.
    // Using vertex ids as tags avoids resetting the whole array each step.
    std::vector<int> forbidden(n + 1, -1);

    for (const int v : order) {
        // Mark colors of already-colored distance-2 neighbours as forbidden.
        auto mark_neighbors = [&](std::span<const int> through_vertices) {
            for (const int w : through_vertices) {
                // w is on the opposite side; iterate w's neighbours on side.
                auto same_side = (side == ColoringSide::COLUMNS)
                    ? g.row_neighbors(w)   // row w → column neighbours
                    : g.col_neighbors(w);  // col w → row neighbours
                for (const int u : same_side) {
                    if (u != v && result.colors[u] != -1) {
                        const int col = result.colors[u];
                        if (col < static_cast<int>(forbidden.size()))
                            forbidden[col] = v; // tag as forbidden for v
                    }
                }
            }
        };

        auto opposite_neighbors = (side == ColoringSide::COLUMNS)
            ? g.col_neighbors(v)
            : g.row_neighbors(v);
        mark_neighbors(opposite_neighbors);

        // Find the minimum non-forbidden color.
        int color = 0;
        while (color < static_cast<int>(forbidden.size()) &&
               forbidden[color] == v)
            ++color;

        result.colors[v] = color;
        result.num_colors = std::max(result.num_colors, color + 1);
    }

    return result;
}

ColoringResult greedy_color(const BipartiteGraph& g,
                            ColoringSide side,
                            OrderingStrategy strategy,
                            unsigned seed) {
    auto order = generate_ordering(g, side, strategy, seed);
    return greedy_color(g, side, order);
}

// ── verification ──────────────────────────────────────────────────────────────

bool verify_coloring(const BipartiteGraph& g, const ColoringResult& res) {
    const int n = (res.side == ColoringSide::COLUMNS) ? g.num_cols()
                                                       : g.num_rows();
    if (static_cast<int>(res.colors.size()) != n) return false;

    for (int v = 0; v < n; ++v) {
        if (res.colors[v] < 0) return false; // uncolored vertex

        auto opposite = (res.side == ColoringSide::COLUMNS)
            ? g.col_neighbors(v)
            : g.row_neighbors(v);

        for (const int w : opposite) {
            auto same_side = (res.side == ColoringSide::COLUMNS)
                ? g.row_neighbors(w)
                : g.col_neighbors(w);

            for (const int u : same_side) {
                if (u != v && res.colors[u] == res.colors[v])
                    return false; // conflict: v and u share neighbour w
            }
        }
    }
    return true;
}

// ── string helpers ────────────────────────────────────────────────────────────

std::string to_string(ColoringSide side) {
    return side == ColoringSide::COLUMNS ? "columns" : "rows";
}

std::string to_string(OrderingStrategy s) {
    switch (s) {
        case OrderingStrategy::NATURAL:              return "natural";
        case OrderingStrategy::RANDOM:               return "random";
        case OrderingStrategy::LARGEST_DEGREE_FIRST: return "largest-degree-first";
        case OrderingStrategy::SMALLEST_DEGREE_FIRST:return "smallest-degree-first";
    }
    return "unknown";
}
