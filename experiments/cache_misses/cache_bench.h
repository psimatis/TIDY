// Shared harness for the cache-miss experiment (Figure 10).
//
// 1. Pins the process to one performance core (CPU 0), so all counts come from the same core's caches.
// 2. Replays the stream through the live index (not measured) to obtain the
//    dead-index inserts (finished intervals) and the queries in stream order.
// 3. Runs two passes over the same interleaved operations:
//      pass 1 measures the inserts (counters and timer paused around queries),
//      pass 2 measures the queries (inserts still run, but are not measured).
// Each phase records wall-clock time and L1/L2/L3 load misses.
#ifndef CACHE_BENCH_H
#define CACHE_BENCH_H

#include "indexes/def_global.h"
#include "indexes/containers/relation.h"
#include "indexes/LIT/live_index.cpp"
#include "experiments/cache_misses/perf_counter.h"

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <chrono>
#include <sched.h>

using Clock = std::chrono::steady_clock;

// Pin this process to one performance core so caches/PMU are consistent.
inline void pin_to_pcore(int cpu = 0) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0)
        std::cerr << "[warn] could not pin to CPU " << cpu << " (continuing unpinned)\n";
}

// One operation in stream order: either an insert (a finished interval) or a query.
struct Op {
    bool is_query;
    RecordId id;        // insert payload: full interval (id, start, end)
    Timestamp start;
    Timestamp end;
    Timestamp qs;       // query payload: range [qs, qe]
    Timestamp qe;
};

inline std::vector<Op> cache_bench_build_ops(const char *streamFile, size_t maxCapacity = 10000) {
    LiveIndex *liveIndex = new LiveIndexCapacityConstraintedICDE16(maxCapacity);
    std::vector<Op> ops;

    std::ifstream fQ(streamFile);
    if (!fQ) {
        std::cerr << "Error: cannot open stream file: " << streamFile << "\n";
        std::exit(1);
    }
    char operation;
    Timestamp first, second, dummy1, dummy2;
    while (fQ >> operation >> first >> second >> dummy1 >> dummy2) {
        switch (operation) {
            case 'S':
                liveIndex->insert(first, second);
                break;
            case 'E': {
                Timestamp startEndpoint = liveIndex->remove(first);
                ops.push_back({false, first, startEndpoint, second, 0, 0});
                break;
            }
            case 'Q':
                ops.push_back({true, 0, 0, 0, first, second});
                break;
        }
    }
    fQ.close();
    delete liveIndex;
    return ops;
}

inline void cache_bench_report(std::ofstream &csv, const std::string &dataset, const std::string &extent,
                               const std::string &index_name, const std::string &phase,
                               size_t num_ops, double time_sec, const CacheStats &s) {
    double per = num_ops ? 1.0 / (double)num_ops : 0.0;
    std::cout << "  [" << phase << "]  ops=" << num_ops
              << "  time=" << time_sec << "s  IPC=" << s.ipc() << "\n";
    std::cout << "      L1 load-miss : " << (uint64_t)s.l1_miss << "   (" << s.l1_miss * per << " /op)\n";
    std::cout << "      L2 load-miss : " << (uint64_t)s.l2_miss << "   (" << s.l2_miss * per << " /op)\n";
    std::cout << "      L3 load-miss : " << (uint64_t)s.l3_miss << "   (" << s.l3_miss * per << " /op)\n";
    std::cout << "      instructions : " << (uint64_t)s.instructions << "   (" << s.instructions * per << " /op)\n";
    std::cout << "      cycles       : " << (uint64_t)s.cycles << "   (" << s.cycles * per << " /op)\n";
    std::cout << "      branch-miss  : " << (uint64_t)s.branch_misses << "   (" << s.branch_misses * per << " /op)\n";

    if (csv.is_open()) {
        csv << dataset << "," << extent << "," << index_name << "," << phase << ","
            << num_ops << "," << time_sec << ","
            << s.l1_miss << "," << s.l2_miss << "," << s.l3_miss << ","
            << s.instructions << "," << s.cycles << "," << s.branch_misses << "\n";
    }
}

// Generic two-pass driver. The four callables are supplied by each main:
//   make_index()  / destroy_index()
//   run_insert(const Op&)        -> insert one finished interval
//   run_query(const Op&) -> size_t -> run one query, return #results (sink)
template <class Make, class Destroy, class Insert, class Query>
inline int cache_bench_run(const char *streamFile, const std::string &index_name,
                           const std::string &dataset, const std::string &extent,
                           Make make_index, Destroy destroy_index,
                           Insert run_insert, Query run_query) {
    pin_to_pcore(0);

    CacheProfiler prof;
    if (!prof.available()) {
        std::cerr << "[perf] Hardware counters are unavailable; aborting.\n";
        return 2;
    }

    std::vector<Op> ops = cache_bench_build_ops(streamFile);

    size_t n_inserts = 0, n_queries = 0;
    for (const Op &op : ops) (op.is_query ? n_queries : n_inserts)++;

    std::cout << "\nCACHE (L1/L2/L3 load misses, P-core pinned)\n";
    std::cout << "====================\n";
    std::cout << "Index   : " << index_name << "\n";
    std::cout << "Dataset : " << dataset << "   Extent: " << extent << "\n";
    std::cout << "Inserts : " << n_inserts << "   Queries: " << n_queries << "\n\n";

    std::ofstream csv("experiments/cache_misses/results.csv", std::ios_base::app);
    size_t result_sink = 0;

    // -------- PASS 1: INSERTS (counter+timer ON, paused around queries) --------
    make_index();
    prof.reset();
    prof.enable();
    auto pass1_start = Clock::now();
    double query_time_in_pass1 = 0.0;
    for (const Op &op : ops) {
        if (op.is_query) {
            prof.disable();
            auto q0 = Clock::now();
            result_sink += run_query(op);
            query_time_in_pass1 += std::chrono::duration<double>(Clock::now() - q0).count();
            prof.enable();
        } else {
            run_insert(op);
        }
    }
    prof.disable();
    double pass1_total = std::chrono::duration<double>(Clock::now() - pass1_start).count();
    double insert_time = pass1_total - query_time_in_pass1;   // time spent in inserts only
    CacheStats insert_stats = prof.read();
    destroy_index();
    cache_bench_report(csv, dataset, extent, index_name, "insert", n_inserts, insert_time, insert_stats);

    // -------- PASS 2: QUERIES (counter+timer ON per query only) --------
    make_index();
    CacheStats query_total;
    double query_time = 0.0;
    prof.disable();
    for (const Op &op : ops) {
        if (op.is_query) {
            prof.reset();
            prof.enable();
            auto q0 = Clock::now();
            result_sink += run_query(op);
            query_time += std::chrono::duration<double>(Clock::now() - q0).count();
            prof.disable();
            CacheStats s = prof.read();
            query_total.l1_miss       += s.l1_miss;
            query_total.l2_miss       += s.l2_miss;
            query_total.l3_miss       += s.l3_miss;
            query_total.instructions  += s.instructions;
            query_total.cycles        += s.cycles;
            query_total.branch_misses += s.branch_misses;
        } else {
            run_insert(op);
        }
    }
    destroy_index();
    cache_bench_report(csv, dataset, extent, index_name, "query", n_queries, query_time, query_total);

    std::cout << "  (result sink = " << result_sink << ")\n\n";
    if (csv.is_open()) csv.close();
    return 0;
}

#endif // CACHE_BENCH_H
