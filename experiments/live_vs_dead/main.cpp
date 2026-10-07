#include "getopt.h"
#include "indexes/def_global.h"
#include "indexes/containers/relation.h"
#include "indexes/LIT/hint_m.h"
#include "indexes/LIT/live_index.cpp"

int main(int argc, char **argv) {
    Timer tim;
    LiveIndex *liveIndex;
    HINT_M_Dynamic *hintIndex;
    size_t totalResults = 0, numQueries = 0, numUpdates = 0;
    size_t numStartUpdates = 0, numEndUpdates = 0;
    size_t liveResult = 0, deadResult = 0;
    size_t totalLiveResult = 0, totalDeadResult = 0;
    double live_insert_time = 0, live_remove_time = 0, live_querytime = 0;
    double dead_insert_time = 0, dead_querytime = 0;
    char c, operation;
    int numRuns = 1;
    char *queryFile;
    Timestamp first, second, dummy1, dummy2;
    Timestamp leafPartitionExtent = 86400;
    size_t maxCapacity = 10000;
    size_t maxNumBuffers = 0;
    bool stabbing = false;

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <streamfile>" << endl;
        return 1;
    }

    queryFile = argv[1];

    liveIndex = new LiveIndexCapacityConstraintedICDE16(maxCapacity);
    hintIndex = new HINT_M_Dynamic(leafPartitionExtent);

    ifstream fQ(queryFile);
    if (!fQ) {
        cerr << "Error: Cannot open stream file: " << queryFile << endl;
        return 1;
    }

    while (fQ >> operation >> first >> second >> dummy1 >> dummy2) {
        switch (operation) {
            case 'S':
                numUpdates++;
                numStartUpdates++;
                tim.start();
                liveIndex->insert(first, second);
                live_insert_time += tim.stop();
                break;
            case 'E': {
                numUpdates++;
                numEndUpdates++;
                tim.start();
                Timestamp startEndpoint = liveIndex->remove(first);
                live_remove_time += tim.stop();
                
                tim.start();
                hintIndex->insert(Record(first, startEndpoint, second));
                dead_insert_time += tim.stop();
                break;
            }
            case 'Q':
                numQueries++;
                for (auto r = 0; r < numRuns; r++) {
#ifdef WORKLOAD_COUNT
                    vector<RecordId> liveResults;
                    tim.start();
                    liveIndex->execute_pureTimeTravel(RangeQuery(numQueries, first, second), liveResults);
                    live_querytime += tim.stop();
                    liveResult = liveResults.size();

                    vector<RecordId> deadResults;
                    tim.start();
                    if (first <= hintIndex->gend)
                        hintIndex->execute_pureTimeTravel(RangeQuery(numQueries, first, second), deadResults);
                    dead_querytime += tim.stop();
                    deadResult = deadResults.size();
#else
                    tim.start();
                    liveResult = liveIndex->execute_pureTimeTravel(RangeQuery(numQueries, first, second));
                    live_querytime += tim.stop();

                    deadResult = 0;
                    tim.start();
                    if (first <= hintIndex->gend)
                        deadResult = hintIndex->execute_pureTimeTravel(RangeQuery(numQueries, first, second));
                    dead_querytime += tim.stop();
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
    cout << "LIT/HINT (Live vs Dead)" << endl;
    cout << "====================" << endl;
    cout << endl;
    cout << "Updates report" << endl;
    cout << "Num of updates                     : " << numUpdates << endl;
    cout << "Num of start updates               : " << numStartUpdates << endl;
    cout << "Num of end updates                 : " << numEndUpdates << endl;
    cout << "Num of buffers  (max)              : " << maxNumBuffers << endl;
    cout << "Total live insert time       [secs]: " << live_insert_time << endl;
    cout << "Total live remove time       [secs]: " << live_remove_time << endl;
    cout << "Total updating time (live)   [secs]: " << live_insert_time + live_remove_time << endl;
    cout << "Total updating time (dead)   [secs]: " << dead_insert_time << endl;
    cout << "Live update percentage         [%] : " << (live_insert_time + live_remove_time) / (live_insert_time + live_remove_time + dead_insert_time) * 100 << endl;
    cout << "Dead update percentage         [%] : " << dead_insert_time / (live_insert_time + live_remove_time + dead_insert_time) * 100 << endl << endl;
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
    cout << "Total querying time (dead)   [secs]: " << dead_querytime/numRuns << endl;
    cout << "Live query percentage          [%] : " << (live_querytime/numRuns) / (live_querytime/numRuns + dead_querytime/numRuns) * 100 << endl;
    cout << "Dead query percentage          [%] : " << (dead_querytime/numRuns) / (live_querytime/numRuns + dead_querytime/numRuns) * 100 << endl << endl;
    cout << "Memory usage" << endl;
    cout << "HINT index size                [MB]: " << hintIndex->getSize() / 1e6 << endl;
    cout << "Live index size                [MB]: " << liveIndex->getMemoryUsage() / 1e6 << endl << endl;

    delete hintIndex;
    delete liveIndex;
    
    return 0;
}
