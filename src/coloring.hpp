#pragma once

#include <span>
#include <string>
#include <vector>

#include "bipartite_graph.hpp"
#include "required_edges.hpp"

// Which partition of the bipartite graph to color.
enum class ColoringSide { ROWS, COLUMNS };

// Vertex ordering strategies applied before the greedy pass.
enum class OrderingStrategy {
    NATURAL,              // 0, 1, 2, …
    RANDOM,               // random permutation
    LARGEST_DEGREE_FIRST, // descending bipartite degree
    SMALLEST_DEGREE_FIRST // ascending bipartite degree
};

struct ColoringResult {
    ColoringSide side;
    std::vector<int> colors;  // colors[v] = assigned color (0-based)
    int num_colors = 0;
};

// Build a vertex ordering for one side of the bipartite graph.
std::vector<int> generate_ordering(const BipartiteGraph& g,
                                   ColoringSide side,
                                   OrderingStrategy strategy,
                                   unsigned seed = 42);

// Greedy distance-2 coloring of `side` in the given vertex order.
// Two vertices on the same side conflict when they share a common neighbour
// on the other side (i.e. they are at distance 2 through the bipartite graph).
ColoringResult greedy_color(const BipartiteGraph& g,
                            ColoringSide side,
                            std::span<const int> order);

// Convenience overload: generate the ordering internally.
ColoringResult greedy_color(const BipartiteGraph& g,
                            ColoringSide side,
                            OrderingStrategy strategy = OrderingStrategy::NATURAL,
                            unsigned seed = 42);

// Returns true iff no two distance-2 neighbours share the same color.
bool verify_coloring(const BipartiteGraph& g, const ColoringResult& result);

// ── restricted coloring ───────────────────────────────────────────────────────
//
// Two vertices on the same side conflict only when they share a common
// neighbour w on the other side AND at least one of the two edges incident
// on w is in `required`.  This is a relaxation of full distance-2 coloring
// and always produces ≤ colors than the unrestricted version.

ColoringResult greedy_color_restricted(const BipartiteGraph& g,
                                       ColoringSide side,
                                       std::span<const int> order,
                                       const RequiredEdges& required);

ColoringResult greedy_color_restricted(const BipartiteGraph& g,
                                       ColoringSide side,
                                       OrderingStrategy strategy,
                                       const RequiredEdges& required,
                                       unsigned seed = 42);

// Returns true iff the coloring satisfies the restricted conflict rule.
bool verify_restricted_coloring(const BipartiteGraph& g,
                                const ColoringResult& result,
                                const RequiredEdges& required);

std::string to_string(ColoringSide side);
std::string to_string(OrderingStrategy strategy);
