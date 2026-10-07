#include "getopt.h"
#include "indexes/def_global.h"
#include "indexes/containers/relation.h"
#include "indexes/rtree.hpp"
#include "indexes/LIT/live_index.cpp"

void usage() {
    cerr << endl;
    cerr << "PROJECT" << endl;
    cerr << "       R-tree" << endl << endl;
    cerr << "USAGE" << endl;
    cerr << "       ./rtree.exe [OPTIONS] [STREAMFILE]" << endl << endl;
    cerr << "DESCRIPTION" << endl;
    cerr << "       -s" << endl;
    cerr << "              use snapshot stabbing queries instead of domain extent" << endl;
    cerr << "       -r runs" << endl;
    cerr << "              set the number of runs per query; by default 1" << endl << endl;
    cerr << "EXAMPLE" << endl;
    cerr << "       ./rtree.exe data/stream/domain_extent_uniform/TAXIS/TAXIS_stream_dom0p01.stream" << endl << endl;
}

int main(int argc, char **argv) {
    Timer tim;
    RTree *rtreeIndex;
    LiveIndex *liveIndex;
    size_t totalResults = 0, numQueries = 0, numUpdates = 0;
    size_t liveResult = 0, deadResult = 0;
    size_t totalLiveResult = 0, totalDeadResult = 0;
    double live_insert_time = 0, live_remove_time = 0, live_querytime = 0, dead_insert_time = 0, dead_querytime = 0;
    char c, operation;
    int numRuns = 1;
    char *queryFile;
    Timestamp first, second, dummy1, dummy2;
    bool stabbing = false;
    size_t maxCapacity = 10000;
    size_t maxNumBuffers = 0;

    while ((c = getopt(argc, argv, "r:s")) != -1) {
        switch (c) {
            case 'r':
                numRuns = atoi(optarg);
                break;
            case 's':
                stabbing = true;
                break;
            default:
                cerr << endl << "Error - unknown option '" << c << "'" << endl << endl;
                usage();
                return 1;
        }
    }
    
    if (argc-optind != 1) {
        usage();
        return 1;
    }

    rtreeIndex = new RTree();
    liveIndex = new LiveIndexCapacityConstraintedICDE16(maxCapacity);

    queryFile = argv[optind];
    ifstream fQ(queryFile);
    if (!fQ) {
        usage();
        return 1;
    }

    while (fQ >> operation >> first >> second >> dummy1 >> dummy2) {
        switch (operation) {
            case 'S':
                numUpdates++;
                tim.start();
                liveIndex->insert(first, second);
                live_insert_time += tim.stop();
                break;
            case 'E': {
                numUpdates++;
                tim.start();
                Timestamp startEndpoint = liveIndex->remove(first);
                live_remove_time += tim.stop();
                
                // Insert the interval into the R-tree
                tim.start();
                rtreeIndex->insert(startEndpoint, second);
                dead_insert_time += tim.stop();
                break;
            }
            case 'Q':
                numQueries++;
                for (auto r = 0; r < numRuns; r++) {
#ifdef WORKLOAD_COUNT
                    Timestamp qend = stabbing ? first : second;
                    vector<RecordId> liveResults;
                    tim.start();
                    liveIndex->execute_pureTimeTravel(RangeQuery(numQueries, first, qend), liveResults);
                    live_querytime += tim.stop();
                    liveResult = liveResults.size();

                    vector<RTreeValue> deadResults;
                    // Query: start <= qend AND end >= first
                    tim.start();
                    rtreeIndex->query(first, qend, deadResults);
                    dead_querytime += tim.stop();
                    deadResult = deadResults.size();
#else
                    Timestamp qend = stabbing ? first : second;
                    tim.start();
                    liveResult = liveIndex->execute_pureTimeTravel(RangeQuery(numQueries, first, qend));
                    live_querytime += tim.stop();

                    vector<RTreeValue> results;
                    // Query: start <= qend AND end >= first
                    tim.start();
                    rtreeIndex->query(first, qend, results);
                    dead_querytime += tim.stop();
                    deadResult = 0;
                    for (const auto &v : results)
                        deadResult ^= v.second;
#endif
                }
#ifdef WORKLOAD_COUNT
                totalLiveResult += liveResult;
                totalDeadResult += deadResult;
                totalResults += liveResult + deadResult;
#else
                totalLiveResult ^= liveResult;
                totalDeadResult ^= deadResult;
                totalResults ^= liveResult ^ deadResult;
#endif
                break;
        }
        maxNumBuffers = max(maxNumBuffers, liveIndex->getNumBuffers());
    }
    fQ.close();
    
    cout << endl;
    cout << "R-tree" << endl;
    cout << "====================" << endl;
    cout << endl;
    cout << "Updates report" << endl;
    cout << "Num of updates                     : " << numUpdates << endl;
    cout << "Num of buffers  (max)              : " << maxNumBuffers << endl;
    cout << "Total updating time (live)   [secs]: " << live_insert_time + live_remove_time << endl;
    cout << "Total updating time (dead)   [secs]: " << dead_insert_time << endl << endl;
    cout << "Queries report" << endl;
    cout << "Num of queries                     : " << numQueries << endl;
    cout << "Num of runs per query              : " << numRuns << endl;
#ifdef WORKLOAD_COUNT
    cout << "Result (live) [COUNT]              : " << totalLiveResult << endl;
    cout << "Result (dead) [COUNT]              : " << totalDeadResult << endl;
    cout << "Total result [COUNT]               : " << totalResults << endl;
#else
    cout << "Result (live) [XOR]                : " << totalLiveResult << endl;
    cout << "Result (dead) [XOR]                : " << totalDeadResult << endl;
    cout << "Total result [XOR]                 : " << totalResults << endl;
#endif
    cout << "Total querying time (live)   [secs]: " << live_querytime/numRuns << endl;
    cout << "Total querying time (dead)   [secs]: " << dead_querytime/numRuns << endl << endl;
    cout << "Memory usage" << endl;
    cout << "R-tree index size              [MB]: " << rtreeIndex->memory_usage_mb() << endl;
    cout << "Live index size                [MB]: " << liveIndex->getMemoryUsage() / 1e6 << endl << endl;

    delete rtreeIndex;
    delete liveIndex;
    
    return 0;
}
