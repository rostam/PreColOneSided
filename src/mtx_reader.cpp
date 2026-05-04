#include "mtx_reader.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <sstream>
#include <fstream>
#include <ranges>
#include <stdexcept>

namespace {

std::string to_lower(std::string s) {
    std::ranges::transform(s, s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

MTXMatrix parse_stream(std::istream& in) {
    std::string line;

    // --- header line ---
    if (!std::getline(in, line))
        throw std::runtime_error("MTX: empty file");

    auto lower = to_lower(line);
    if (!lower.starts_with("%%matrixmarket"))
        throw std::runtime_error("MTX: missing %%MatrixMarket banner");

    std::istringstream hdr(lower);
    std::vector<std::string> tokens;
    for (std::string t; hdr >> t;)
        tokens.push_back(std::move(t));

    if (tokens.size() < 5)
        throw std::runtime_error("MTX: incomplete header");
    if (tokens[1] != "matrix")
        throw std::runtime_error("MTX: only 'matrix' object supported");
    if (tokens[2] != "coordinate")
        throw std::runtime_error("MTX: only 'coordinate' format supported");

    MTXType type = MTXType::REAL;
    if      (tokens[3] == "pattern") type = MTXType::PATTERN;
    else if (tokens[3] == "integer") type = MTXType::INTEGER;
    else if (tokens[3] == "complex") type = MTXType::COMPLEX;

    MTXSymmetry sym = MTXSymmetry::GENERAL;
    if      (tokens[4] == "symmetric")     sym = MTXSymmetry::SYMMETRIC;
    else if (tokens[4] == "skew-symmetric") sym = MTXSymmetry::SKEW_SYMMETRIC;
    else if (tokens[4] == "hermitian")     sym = MTXSymmetry::HERMITIAN;

    // --- skip comment lines ---
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line[0] != '%') break;
    }

    // --- size line ---
    int m, n, nnz;
    {
        std::istringstream ss(line);
        if (!(ss >> m >> n >> nnz))
            throw std::runtime_error("MTX: failed to parse size line");
    }
    if (m <= 0 || n <= 0 || nnz < 0)
        throw std::runtime_error("MTX: invalid dimensions");

    MTXMatrix mat;
    mat.rows     = m;
    mat.cols     = n;
    mat.symmetry = sym;
    mat.row_indices.reserve(nnz * (sym != MTXSymmetry::GENERAL ? 2 : 1));
    mat.col_indices.reserve(nnz * (sym != MTXSymmetry::GENERAL ? 2 : 1));

    // --- data lines ---
    for (int k = 0; k < nnz; ++k) {
        if (!std::getline(in, line))
            throw std::runtime_error(
                std::format("MTX: unexpected EOF at entry {}/{}", k, nnz));

        // skip blank / comment lines that some generators emit inside data
        while ((line.empty() || line[0] == '%') && std::getline(in, line)) {}

        std::istringstream ss(line);
        int r, c;
        if (!(ss >> r >> c))
            throw std::runtime_error(
                std::format("MTX: cannot parse row/col at entry {}", k + 1));

        if (r < 1 || r > m || c < 1 || c > n)
            throw std::runtime_error(
                std::format("MTX: index ({},{}) out of range {}x{}", r, c, m, n));

        // convert to 0-based
        mat.row_indices.push_back(r - 1);
        mat.col_indices.push_back(c - 1);

        // expand symmetric/hermitian off-diagonal entries
        if (sym != MTXSymmetry::GENERAL && r != c) {
            mat.row_indices.push_back(c - 1);
            mat.col_indices.push_back(r - 1);
        }
    }

    (void)type; // values not needed for structural coloring
    return mat;
}

} // anonymous namespace

MTXMatrix read_mtx(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error(
            std::format("MTX: cannot open '{}'", path.string()));
    return parse_stream(file);
}

MTXMatrix read_mtx_string(const std::string& content) {
    std::istringstream ss(content);
    return parse_stream(ss);
}
