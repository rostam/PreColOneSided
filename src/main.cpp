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

namespace {

void print_usage(std::string_view prog) {
    std::cout << std::format(
        "Usage: {} <matrix.mtx> [options]\n"
        "\nOptions:\n"
        "  --side    rows|columns     Which side to color (default: columns)\n"
        "  --order   natural|random|largest|smallest\n"
        "                             Vertex ordering strategy (default: natural)\n"
        "  --seed    <uint>           RNG seed for random ordering (default: 42)\n"
        "  --verify                  Verify the coloring after computing it\n",
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
    ColoringSide    side     = ColoringSide::COLUMNS;
    OrderingStrategy strategy = OrderingStrategy::NATURAL;
    unsigned        seed     = 42;
    bool            verify   = false;

    try {
        mtx_path = argv[1];
        for (int i = 2; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "--side"  && i + 1 < argc) side     = parse_side    (argv[++i]);
            else if (arg == "--order" && i + 1 < argc) strategy = parse_strategy(argv[++i]);
            else if (arg == "--seed"  && i + 1 < argc) seed     = static_cast<unsigned>(std::stoul(argv[++i]));
            else if (arg == "--verify") verify = true;
            else if (arg == "--help")  { print_usage(argv[0]); return 0; }
            else throw std::invalid_argument(std::format("unknown argument '{}'", arg));
        }

        // Load matrix
        auto t0 = std::chrono::steady_clock::now();
        const auto mat = read_mtx(mtx_path);
        const BipartiteGraph g(mat);

        std::cout << std::format(
            "Matrix: {}  ({} rows × {} cols, {} structural non-zeros)\n",
            mtx_path.filename().string(),
            g.num_rows(), g.num_cols(),
            static_cast<int>(mat.row_indices.size()));

        // Color
        auto t1 = std::chrono::steady_clock::now();
        const auto result = greedy_color(g, side, strategy, seed);
        auto t2 = std::chrono::steady_clock::now();

        double load_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double color_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

        std::cout << std::format(
            "Side: {}  |  Order: {}  |  Colors used: {}\n"
            "Load: {:.2f} ms  |  Coloring: {:.2f} ms\n",
            to_string(side), to_string(strategy),
            result.num_colors,
            load_ms, color_ms);

        if (verify) {
            const bool ok = verify_coloring(g, result);
            std::cout << std::format("Verification: {}\n", ok ? "PASSED" : "FAILED");
            if (!ok) return 2;
        }

    } catch (const std::exception& e) {
        std::cerr << std::format("Error: {}\n", e.what());
        return 1;
    }

    return 0;
}
