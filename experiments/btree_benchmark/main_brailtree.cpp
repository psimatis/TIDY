#include <iostream>
#include <fstream>
#include <vector>
#include <getopt.h>
#include "../../indexes/bplustrees/brail.hpp"

using namespace std;

int main(int argc, char **argv) {
    int numRuns = 1;
    bool stabbing = false;
    char c;
    while ((c = getopt(argc, argv, "r:s")) != -1) {
        switch (c) {
            case 'r': numRuns = atoi(optarg); break;
            case 's': stabbing = true; break;
        }
    }

    if (optind >= argc) {
        cerr << "Usage: " << argv[0] << " -r [RUNS] [STREAMFILE]" << endl;
        return 1;
    }

    ifstream fQ(argv[optind]);
    if (!fQ) { cerr << "Error opening file." << endl; return 1; }

    Brail tree;
    char op;
    Timestamp d1, d2;
    size_t numUpdates = 0, numQueries = 0, totalResult = 0;
    Timer tim;
    double update_time = 0, query_time = 0;

    Timestamp col1, col2;
    while (fQ >> op >> col1 >> col2 >> d1 >> d2) {
        if (op == 'E') {
            Timestamp id = col1, end_time = col2;
            numUpdates++;
            tim.start();
            tree.insert(end_time, id);
            update_time += tim.stop();
        } else if (op == 'Q') {
            Timestamp q_start = col1, q_end = col2;
            numQueries++;
            size_t result_count = 0;
            for (int r = 0; r < numRuns; r++) {
                size_t first, last;
                if (stabbing) {
                    tim.start();
                    first = tree.find_position(q_start, true);
                    last = first;
                    while (last < tree.size && tree.keys[last] == q_start)
                        last++;
                    query_time += tim.stop();
                } else {
                    tim.start();
                    first = tree.find_position(q_start, true);
                    last = tree.find_position(q_end, false);
                    query_time += tim.stop();
                }
                result_count = last - first;
            }
            totalResult += result_count;
        }
    }

    cout << (stabbing ? "BrailTreeOptFirst-STAB" : "BrailTreeOptFirst") << endl;
    cout << "Num of updates                     : " << numUpdates << endl;
    cout << "Total updating time (dead)   [secs]: " << update_time << endl;
    cout << "Num of queries                     : " << numQueries << endl;
    cout << "Total result                       : " << totalResult << endl;
    cout << "Total querying time (dead)   [secs]: " << query_time/numRuns << endl;
    cout << "Index size                    [MB]: " << tree.getSize() / 1e6 << endl;

    return 0;
}
