#include "getopt.h"
#include "../../indexes/def_global.h"
#include "../../indexes/containers/relation.h"
#include "../../indexes/bplustrees/brail.hpp"
#include "../../indexes/bplustrees/brail_nomax.hpp"
#include "../../indexes/LIT/live_index.cpp"

int main(int argc, char **argv) {
    Timer tim;
    Brail *brailFull;
    BrailNoMax *brailNoMax;
    LiveIndex *liveIndex;
    size_t numQueries = 0, numUpdates = 0;
    double insert_time_full = 0, insert_time_nomax = 0;
    double q1_time = 0, q2_time = 0, query_time_total = 0;
    double live_insert_time = 0, live_remove_time = 0, live_querytime = 0;
    char c, operation;
    int numRuns = 1;
    char *queryFile;
    Timestamp first, second, dummy1, dummy2;
    bool stabbing = false;
    string variant = "full";
    size_t maxCapacity = 10000;

    while ((c = getopt(argc, argv, "r:sv:")) != -1) {
        switch (c) {
            case 'r':
                numRuns = atoi(optarg);
                break;
            case 's':
                stabbing = true;
                break;
            case 'v':
                variant = optarg;
                break;
            default:
                return 1;
        }
    }
    
    if (argc - optind != 1 || (variant != "full" && variant != "nomax")) {
        cerr << "Usage: " << argv[0] << " -v <full|nomax> [-r runs] [-s] <query_file>" << endl;
        return 1;
    }

    if (variant == "full")
        brailFull = new Brail();
    else
        brailNoMax = new BrailNoMax();
    liveIndex = new LiveIndexCapacityConstraintedICDE16(maxCapacity);

    queryFile = argv[optind];
    ifstream fQ(queryFile);
    if (!fQ) {
        cerr << "Cannot open file: " << queryFile << endl;
        return 1;
    }

    // Dead records in arrival order, replayed after the stream to time updates in isolation
    struct DeadRecord { Timestamp end, id, dur; };
    vector<DeadRecord> deadRecords;

    size_t result_count = 0, live_result = 0;
    size_t total_result = 0, total_live_result = 0;

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
                
                deadRecords.push_back({second, first, dur});
                if (variant == "full") {
                    tim.start();
                    brailFull->insert(second, first, dur);
                    insert_time_full += tim.stop();
                } else {
                    tim.start();
                    brailNoMax->insert(second, first, dur);
                    insert_time_nomax += tim.stop();
                }
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
                    live_result = liveResults.size();
                    
                    if (variant == "full") {
                        vector<Timestamp> results;
                        if (stabbing) {
                            tim.start();
                            brailFull->q2_no_up(first, results);
                            q2_time += tim.stop();
                        } else {
                            tim.start();
                            brailFull->q2_no_up(second, results);
                            q2_time += tim.stop();
                            tim.start();
                            brailFull->q1(first, second, results);
                            q1_time += tim.stop();
                        }
                        result_count = results.size();
                    } else {
                        vector<Timestamp> results;
                        if (stabbing) {
                            tim.start();
                            brailNoMax->q2(first, results);
                            q2_time += tim.stop();
                        } else {
                            tim.start();
                            brailNoMax->q2(second, results);
                            q2_time += tim.stop();
                            tim.start();
                            brailNoMax->q1(first, second, results);
                            q1_time += tim.stop();
                        }
                        result_count = results.size();
                    }
                }
                total_live_result += live_result;
                total_result += result_count;
                break;
        }
    }
    fQ.close();

    // Two update times are reported:
    //  - "Total updating time (dead)": each insert timed inside the stream, as in all
    //    other experiments. The competitors figure uses this for B-Rail.
    //  - "Isolated updating time (dead)": the index rebuilt from the same records with
    //    one timer and no queries. Figure 6a uses this. The maxd bookkeeping costs ~1ns
    //    per insert, far below the per-insert timer overhead (~20ns) and the cache
    //    effects of the interleaved queries (NoMax scans to the end of the array, Full
    //    prunes), so the in-stream number can even show Full as cheaper than NoMax.
    double isolated_insert_time;
    if (variant == "full") {
        delete brailFull;
        brailFull = new Brail();
        tim.start();
        for (const auto &r : deadRecords)
            brailFull->insert(r.end, r.id, r.dur);
        isolated_insert_time = tim.stop();
    } else {
        delete brailNoMax;
        brailNoMax = new BrailNoMax();
        tim.start();
        for (const auto &r : deadRecords)
            brailNoMax->insert(r.end, r.id, r.dur);
        isolated_insert_time = tim.stop();
    }
    vector<DeadRecord>().swap(deadRecords);
    
    query_time_total = q1_time + q2_time;
    double insert_time = (variant == "full") ? insert_time_full : insert_time_nomax;
    
    cout << endl;
    cout << (stabbing ? "BRAIL-VARIANTS-STAB" : "BRAIL-VARIANTS") << endl;
    cout << "====================" << endl;
    cout << "Num of updates                     : " << numUpdates << endl;
    cout << "Num of queries                     : " << numQueries << endl;
    cout << endl;
    
    if (variant == "nomax")
        cout << "--- TIDY-NoMax (no pruning) ---" << endl;
    else
        cout << "--- TIDY-Full (full pruning) ---" << endl;
    cout << "Total updating time (dead)   [secs]: " << insert_time << endl;
    cout << "Isolated updating time (dead)[secs]: " << isolated_insert_time << endl;
    cout << "Total querying time Q1       [secs]: " << q1_time << endl;
    cout << "Total querying time Q2       [secs]: " << q2_time << endl;
    cout << "Total result [COUNT]               : " << total_result << endl;
    if (variant == "full")
        cout << "Index size                    [MB]: " << brailFull->getSize() / 1e6 << endl;
    else
        cout << "Index size                    [MB]: " << brailNoMax->getSize() / 1e6 << endl;
    cout << endl;
    
    cout << "--- Live Index ---" << endl;
    cout << "Total updating time (live)   [secs]: " << live_insert_time + live_remove_time << endl;
    cout << "Total querying time (live)   [secs]: " << live_querytime << endl;
    cout << "Total result (live) [COUNT]        : " << total_live_result << endl;
    cout << "Index size                    [MB]: " << liveIndex->getMemoryUsage() / 1e6 << endl;
    cout << endl;
    
    if (variant == "full") delete brailFull;
    else delete brailNoMax;
    delete liveIndex;
    
    return 0;
}
