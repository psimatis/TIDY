#include <iostream>
#include <fstream>
#include <vector>
#include <getopt.h>
#include "../../indexes/def_global.h"
#include "tlx/container/btree_multimap.hpp"

using namespace std;

template <typename Key, typename Value>
struct btree_traits_4k {
    static const bool self_verify = false;
    static const bool debug = false;
    static const int leaf_slots = 8192 / sizeof(Value);
    static const int inner_slots = 8192 / (sizeof(Key) + sizeof(void*));
    static const size_t binsearch_threshold = 256;
};

using value_type = pair<Timestamp, Timestamp>;
using btree_multimap_4k = tlx::btree_multimap<Timestamp, Timestamp, less<Timestamp>, btree_traits_4k<Timestamp, value_type>>;

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

    btree_multimap_4k tree;
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
            tree.insert(make_pair(end_time, id));
            update_time += tim.stop();
        } else if (op == 'Q') {
            Timestamp q_start = col1, q_end = col2;
            if (stabbing) q_end = q_start;
            numQueries++;
            vector<Timestamp> results;
            for (int r = 0; r < numRuns; r++) {
                results.clear();
                tim.start();
                tree.range_query(q_start, q_end, results);
                query_time += tim.stop();
            }
            totalResult += results.size();
        }
    }

    cout << (stabbing ? "TLX-multimap-STAB" : "TLX-multimap") << endl;
    cout << "Num of updates                     : " << numUpdates << endl;
    cout << "Total updating time (dead)   [secs]: " << update_time << endl;
    cout << "Num of queries                     : " << numQueries << endl;
    cout << "Total result                       : " << totalResult << endl;
    cout << "Total querying time (dead)   [secs]: " << query_time/numRuns << endl;
    cout << "Index size                    [MB]: " << tree.getSize() / 1e6 << endl;

    return 0;
}
