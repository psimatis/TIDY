// Cache-miss benchmark for TIDY-Lrn (short and long B-Rails, learned delta).
#include "getopt.h"
#include "indexes/tidy/tidy_learned.hpp"
#include "experiments/cache_misses/cache_bench.h"

int main(int argc, char **argv) {
    char c;
    std::string dataset = "unknown", extent = "unknown";
    double outlier_fraction = 0.01;
    double learning_rate = 100;
    size_t warmup_M = 10000;
    while ((c = getopt(argc, argv, "f:l:m:D:E:")) != -1) {
        switch (c) {
            case 'f': outlier_fraction = atof(optarg); break;
            case 'l': learning_rate = atof(optarg); break;
            case 'm': warmup_M = atoi(optarg); break;
            case 'D': dataset = optarg; break;
            case 'E': extent = optarg; break;
            default:
                std::cerr << "Usage: " << argv[0] << " [-f frac -l lr -m M] [-D dataset -E extent] <stream_file>\n";
                return 1;
        }
    }
    if (argc - optind != 1) {
        std::cerr << "Error: expected exactly one stream file argument.\n";
        return 1;
    }
    const char *streamFile = argv[optind];

    TIDY_LEARNED *learned = nullptr;
    return cache_bench_run(
        streamFile, "TIDY-LEARNED", dataset, extent,
        [&]() { learned = new TIDY_LEARNED(outlier_fraction, learning_rate, warmup_M); },
        [&]() { delete learned; learned = nullptr; },
        [&](const Op &op) { learned->insert(Record(op.id, op.start, op.end)); },
        [&](const Op &op) -> size_t {
            std::vector<RecordId> results;
            learned->query_opt(op.qs, op.qe, results);
            return results.size();
        });
}
