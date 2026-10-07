#include "getopt.h"
#include "indexes/def_global.h"
#include "indexes/containers/relation.h"
#include "indexes/bplustrees/brail.hpp"
#include "indexes/LIT/live_index.cpp"

int main(int argc, char **argv) {
    Timer tim;
    LiveIndex *liveIndex;
    Brail *deadIndex;
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
                return 1;
        }
    }
    
    if (argc-optind != 1) return 1;

    deadIndex = new Brail();
    liveIndex = new LiveIndexCapacityConstraintedICDE16(maxCapacity);

    queryFile = argv[optind];
    ifstream fQ(queryFile);
    if (!fQ) return 1;

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
                
                Timestamp dur = second - startEndpoint;
                
                tim.start();
                deadIndex->insert(second, first, dur);
                dead_insert_time += tim.stop();
                break;
            }
            case 'Q':
                numQueries++;
                for (auto r = 0; r < numRuns; r++) {
                    vector<RecordId> liveResults;
                    tim.start();
                    if (stabbing)
                        liveIndex->execute_pureTimeTravel(RangeQuery(numQueries, first, first), liveResults);
                    else
                        liveIndex->execute_pureTimeTravel(RangeQuery(numQueries, first, second), liveResults);
                    live_querytime += tim.stop();
                    liveResult = liveResults.size();

                    vector<Timestamp> deadResults;
                    tim.start();
                    if (stabbing) {
                        deadIndex->q2_no_up(first, deadResults);
                    } else {
                        deadIndex->q1(first, second, deadResults);
                        deadIndex->q2_no_up(second, deadResults);
                    }
                    dead_querytime += tim.stop();
                    deadResult = deadResults.size();
                }
                totalLiveResult += liveResult;
                totalDeadResult += deadResult;
                totalResults += liveResult + deadResult;
                break;
        }
        maxNumBuffers = max(maxNumBuffers, liveIndex->getNumBuffers());
    }
    fQ.close();
    
    cout << endl;
    cout << (stabbing ? "BRAIL-STAB" : "BRAIL") << endl;
    cout << "====================" << endl;
    cout << endl;
    cout << "Updates report" << endl;
    cout << "Num of updates                     : " << numUpdates << endl;
    cout << "Total updating time (live)   [secs]: " << live_insert_time + live_remove_time << endl;
    cout << "Total updating time (dead)   [secs]: " << dead_insert_time << endl;
    cout << endl;
    cout << "Queries report" << endl;
    cout << "Num of queries                     : " << numQueries << endl;
    cout << "Total result [COUNT]               : " << totalResults << endl;
    cout << "Total querying time (live)   [secs]: " << live_querytime << endl;
    cout << "Total querying time (dead)   [secs]: " << dead_querytime << endl;
    cout << endl;
    cout << "Memory usage" << endl;
    cout << "BRAIL index size               [MB]: " << deadIndex->getSize() / 1e6 << endl;
    cout << "Live index size                [MB]: " << liveIndex->getMemoryUsage() / 1e6 << endl << endl;
    
    delete deadIndex;
    delete liveIndex;
    
    return 0;
}
