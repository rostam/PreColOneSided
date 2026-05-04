#pragma once

#include <filesystem>
#include <string>
#include <vector>

enum class MTXSymmetry { GENERAL, SYMMETRIC, SKEW_SYMMETRIC, HERMITIAN };
enum class MTXType { REAL, INTEGER, COMPLEX, PATTERN };

// Holds the sparsity structure (values are ignored).
// All indices are 0-based.
struct MTXMatrix {
    int rows = 0;
    int cols = 0;
    std::vector<int> row_indices;
    std::vector<int> col_indices;
    MTXSymmetry symmetry = MTXSymmetry::GENERAL;
};

// Parse a MatrixMarket coordinate file.
// Symmetric/hermitian entries are expanded to both (i,j) and (j,i).
// Throws std::runtime_error on malformed input.
MTXMatrix read_mtx(const std::filesystem::path& path);

// Parse from an in-memory string (useful in tests).
MTXMatrix read_mtx_string(const std::string& content);
