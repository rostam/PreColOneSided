#include "required_edges.hpp"

#include <stdexcept>

RequiredEdges diagonal_block_required(const BipartiteGraph& g, int block_size) {
    if (block_size <= 0)
        throw std::invalid_argument("block_size must be positive");

    RequiredEdges req;
    const int n_blocks = std::min(g.num_rows(), g.num_cols()) / block_size;

    for (int b = 0; b < n_blocks; ++b) {
        const int r0 = b * block_size, r1 = r0 + block_size;
        const int c0 = b * block_size, c1 = c0 + block_size;

        for (int r = r0; r < r1; ++r) {
            for (int c : g.row_neighbors(r)) {
                if (c >= c0 && c < c1)
                    req.add(r, c);
            }
        }
    }
    return req;
}
