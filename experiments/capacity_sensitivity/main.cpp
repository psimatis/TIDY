#include "getopt.h"
#include "indexes/def_global.h"
#include "indexes/containers/relation.h"
#include "indexes/bplustrees/brail.hpp"
#include "indexes/LIT/live_index.cpp"

int main(int argc, char **argv) {
    Timer tim;
    Brail *brailFull;
    LiveIndex *liveIndex;
    size_t numQueries = 0, numUpdates = 0;
    double insert_time = 0;
    double q1_time = 0, q2_time = 0;
    double live_insert_time = 0, live_remove_time = 0;
    char operation;
    char *queryFile;
    Timestamp first, second, dummy1, dummy2;
    size_t maxCapacity = 10000;

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <streamfile>" << endl;
        return 1;
    }

    queryFile = argv[1];

    brailFull = new Brail();
    liveIndex = new LiveIndexCapacityConstraintedICDE16(maxCapacity);

    ifstream fQ(queryFile);
    if (!fQ) {
        cerr << "Cannot open file: " << queryFile << endl;
        return 1;
    }

    size_t total_result = 0;

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
                brailFull->insert(second, first, dur);
                insert_time += tim.stop();
                break;
            }
            case 'Q': {
                numQueries++;
                vector<Timestamp> results;
                tim.start();
                brailFull->q2_no_up(second, results);
                q2_time += tim.stop();
                tim.start();
                brailFull->q1(first, second, results);
                q1_time += tim.stop();
                total_result += results.size();
                break;
            }
        }
    }
    fQ.close();

    cout << endl;
    cout << "CAPACITY-SENSITIVITY" << endl;
    cout << "====================" << endl;
    cout << "B-rail Capacity                    : " << CAPACITY << endl;
    cout << "Num of updates                     : " << numUpdates << endl;
    cout << "Num of queries                     : " << numQueries << endl;
    cout << endl;
    cout << "Total updating time (dead)   [secs]: " << insert_time << endl;
    cout << "Total querying time Q1       [secs]: " << q1_time << endl;
    cout << "Total querying time Q2       [secs]: " << q2_time << endl;
    cout << "Total querying time          [secs]: " << q1_time + q2_time << endl;
    cout << "Total result [COUNT]               : " << total_result << endl;
    cout << "Index size                    [MB]: " << brailFull->getSize() / 1e6 << endl;
    cout << endl;

    delete brailFull;
    delete liveIndex;

    return 0;
}
