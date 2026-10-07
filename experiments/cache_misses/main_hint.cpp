// Cache-miss benchmark for HINT_M_Dynamic (the LIT dead index).
#include "getopt.h"
#include "indexes/LIT/hint_m.h"
#include "experiments/cache_misses/cache_bench.h"

int main(int argc, char **argv) {
    char c;
    std::string dataset = "unknown", extent = "unknown";
    Timestamp leafPartitionExtent = 86400;
    while ((c = getopt(argc, argv, "e:D:E:")) != -1) {
        switch (c) {
            case 'e': leafPartitionExtent = atoi(optarg); break;
            case 'D': dataset = optarg; break;
            case 'E': extent = optarg; break;
            default:
                std::cerr << "Usage: " << argv[0] << " [-e leafExtent] [-D dataset -E extent] <stream_file>\n";
                return 1;
        }
    }
    if (argc - optind != 1) {
        std::cerr << "Error: expected exactly one stream file argument.\n";
        return 1;
    }
    const char *streamFile = argv[optind];

    HINT_M_Dynamic *hint = nullptr;
    return cache_bench_run(
        streamFile, "HINT", dataset, extent,
        [&]() { hint = new HINT_M_Dynamic(leafPartitionExtent); },
        [&]() { delete hint; hint = nullptr; },
        [&](const Op &op) { hint->insert(Record(op.id, op.start, op.end)); },
        [&](const Op &op) -> size_t {
            std::vector<RecordId> results;
            if (op.qs <= hint->gend)
                hint->execute_pureTimeTravel(RangeQuery(0, op.qs, op.qe), results);
            return results.size();
        });
}
