"""
Batch column/row partial-distance-2 coloring via SparseMatrixColorings.jl.
Each argument is a path to an .mtx file.
Outputs one CSV row per (matrix, side, ordering) combination to stdout.
Header line is printed first.
"""

using SparseMatrixColorings, MatrixMarket, SparseArrays

const ORDERINGS = [
    ("natural",       NaturalOrder()),
    ("largest_first", LargestFirst()),
    ("smallest_last", SmallestLast()),
]

const SIDES = [
    ("column", :column),
    ("row",    :row),
]

function color_one(A, partition, order)
    prob = ColoringProblem(; structure = :nonsymmetric, partition)
    algo = GreedyColoringAlgorithm(order; decompression = :direct)
    t = @elapsed result = coloring(A, prob, algo)
    return ncolors(result), t * 1000.0   # ms
end

function main()
    files = ARGS
    isempty(files) && (println(stderr, "Usage: julia julia_color.jl file1.mtx ..."); exit(1))

    println("matrix,rows,cols,nnz,tool,side,ordering,num_colors,time_ms")
    flush(stdout)

    # Warmup: compile all code paths before measuring
    let A0 = mmread(files[1])
        for (_, part) in SIDES, (_, ord) in ORDERINGS
            try color_one(A0, part, ord) catch end
        end
    end

    for fpath in files
        bname = basename(fpath)
        local A, m, n, nz
        try
            A  = mmread(fpath)
            m, n = size(A)
            nz = nnz(A)
        catch e
            println(stderr, "SKIP $bname: $e")
            continue
        end

        for (side_str, part) in SIDES
            for (ord_name, ord) in ORDERINGS
                try
                    nc, t = color_one(A, part, ord)
                    println("$bname,$m,$n,$nz,julia,$side_str,$ord_name,$nc,$(round(t, digits=3))")
                catch e
                    println("$bname,$m,$n,$nz,julia,$side_str,$ord_name,ERROR,ERROR")
                end
                flush(stdout)
            end
        end
    end
end

main()
