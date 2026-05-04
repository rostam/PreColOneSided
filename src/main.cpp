#include <chrono>
#include <filesystem>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "bipartite_graph.hpp"
#include "coloring.hpp"
#include "mtx_reader.hpp"
#include "required_edges.hpp"

namespace {

void print_usage(std::string_view prog) {
    std::cout << std::format(
        "Usage: {} <matrix.mtx> [options]\n"
        "\nOptions:\n"
        "  --side            rows|columns     Which side to color (default: columns)\n"
        "  --order           natural|random|largest|smallest\n"
        "                                     Vertex ordering (default: natural)\n"
        "  --seed            <uint>           RNG seed for random ordering (default: 42)\n"
        "  --verify                           Verify the coloring\n"
        "  --required-blocks <size>           Also run restricted coloring using\n"
        "                                     diagonal blocks of given size\n",
        prog);
}

ColoringSide parse_side(std::string_view s) {
    if (s == "rows")    return ColoringSide::ROWS;
    if (s == "columns") return ColoringSide::COLUMNS;
    throw std::invalid_argument(std::format("unknown side '{}'; use rows|columns", s));
}

OrderingStrategy parse_strategy(std::string_view s) {
    if (s == "natural")  return OrderingStrategy::NATURAL;
    if (s == "random")   return OrderingStrategy::RANDOM;
    if (s == "largest")  return OrderingStrategy::LARGEST_DEGREE_FIRST;
    if (s == "smallest") return OrderingStrategy::SMALLEST_DEGREE_FIRST;
    throw std::invalid_argument(
        std::format("unknown order '{}'; use natural|random|largest|smallest", s));
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    if (argc < 2) { print_usage(argv[0]); return 1; }

    std::filesystem::path mtx_path;
    ColoringSide     side        = ColoringSide::COLUMNS;
    OrderingStrategy strategy    = OrderingStrategy::NATURAL;
    unsigned         seed        = 42;
    bool             verify      = false;
    int              block_size  = 0;   // 0 = no restricted coloring

    try {
        mtx_path = argv[1];
        for (int i = 2; i < argc; ++i) {
            std::string_view arg = argv[i];
            if      (arg == "--side"  && i+1 < argc) side       = parse_side    (argv[++i]);
            else if (arg == "--order" && i+1 < argc) strategy   = parse_strategy(argv[++i]);
            else if (arg == "--seed"  && i+1 < argc) seed       = static_cast<unsigned>(std::stoul(argv[++i]));
            else if (arg == "--required-blocks" && i+1 < argc)  block_size = std::stoi(argv[++i]);
            else if (arg == "--verify") verify = true;
            else if (arg == "--help")  { print_usage(argv[0]); return 0; }
            else throw std::invalid_argument(std::format("unknown argument '{}'", arg));
        }

        auto t0 = std::chrono::steady_clock::now();
        const auto mat = read_mtx(mtx_path);
        const BipartiteGraph g(mat);

        std::cout << std::format(
            "Matrix: {}  ({} rows × {} cols, {} structural non-zeros)\n",
            mtx_path.filename().string(),
            g.num_rows(), g.num_cols(),
            static_cast<int>(mat.row_indices.size()));

        const auto order = generate_ordering(g, side, strategy, seed);

        // ── regular coloring ──────────────────────────────────────────────────
        auto t1 = std::chrono::steady_clock::now();
        const auto reg = greedy_color(g, side, order);
        auto t2 = std::chrono::steady_clock::now();

        double load_ms  = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double reg_ms   = std::chrono::duration<double, std::milli>(t2 - t1).count();

        std::cout << std::format(
            "Side: {}  |  Order: {}\n"
            "Regular:    Colors used: {}  |  Coloring: {:.3f} ms\n",
            to_string(side), to_string(strategy),
            reg.num_colors, reg_ms);

        if (verify) {
            std::cout << std::format("Regular verification: {}\n",
                verify_coloring(g, reg) ? "PASSED" : "FAILED");
        }

        // ── restricted coloring (optional) ────────────────────────────────────
        if (block_size > 0) {
            auto t3 = std::chrono::steady_clock::now();
            const auto req = diagonal_block_required(g, block_size);
            const auto res = greedy_color_restricted(g, side, order, req);
            auto t4 = std::chrono::steady_clock::now();

            double req_ms = std::chrono::duration<double, std::milli>(t4 - t3).count();
            double reduction = 100.0 * (1.0 - static_cast<double>(res.num_colors)
                                              / reg.num_colors);

            std::cout << std::format(
                "Required edges: {} (diagonal blocks, size={})\n"
                "Restricted: Colors used: {}  |  Coloring: {:.3f} ms"
                "  |  Reduction: {:.1f}%\n",
                req.size(), block_size,
                res.num_colors, req_ms, reduction);

            if (verify) {
                std::cout << std::format("Restricted verification: {}\n",
                    verify_restricted_coloring(g, res, req) ? "PASSED" : "FAILED");
            }
        }

        std::cout << std::format("Load: {:.3f} ms\n", load_ms);

    } catch (const std::exception& e) {
        std::cerr << std::format("Error: {}\n", e.what());
        return 1;
    }

    return 0;
}
