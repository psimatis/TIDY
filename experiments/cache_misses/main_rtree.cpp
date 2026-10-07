// Cache-miss benchmark for the R*-tree.
#include "getopt.h"
#include "indexes/rtree.hpp"
#include "experiments/cache_misses/cache_bench.h"

int main(int argc, char **argv) {
    char c;
    std::string dataset = "unknown", extent = "unknown";
    while ((c = getopt(argc, argv, "D:E:")) != -1) {
        switch (c) {
            case 'D': dataset = optarg; break;
            case 'E': extent = optarg; break;
            default:
                std::cerr << "Usage: " << argv[0] << " [-D dataset -E extent] <stream_file>\n";
                return 1;
        }
    }
    if (argc - optind != 1) {
        std::cerr << "Error: expected exactly one stream file argument.\n";
        return 1;
    }
    const char *streamFile = argv[optind];

    RTree *rtree = nullptr;
    return cache_bench_run(
        streamFile, "R-tree", dataset, extent,
        [&]() { rtree = new RTree(); },
        [&]() { delete rtree; rtree = nullptr; },
        [&](const Op &op) { rtree->insert(op.start, op.end); },
        [&](const Op &op) -> size_t {
            std::vector<RTreeValue> results;
            rtree->query(op.qs, op.qe, results);
            return results.size();
        });
}
