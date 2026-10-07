#include "getopt.h"
#include "indexes/def_global.h"
#include "indexes/containers/relation.h"
#include "indexes/bplustrees/brail.hpp"

int main(int argc, char **argv) {
    Timer tim;
    Brail *brailIndex;
    char c, operation;
    char *queryFile;
    Timestamp first, second, dummy1, dummy2;
    
    vector<tuple<char, Timestamp, Timestamp>> operations;
    
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <query_file>" << endl;
        return 1;
    }

    queryFile = argv[1];
    ifstream fQ(queryFile);
    if (!fQ) {
        cerr << "Cannot open file: " << queryFile << endl;
        return 1;
    }

    while (fQ >> operation >> first >> second >> dummy1 >> dummy2) {
        if (operation == 'E') {
            operations.push_back(make_tuple(operation, first, second));
        }
    }
    fQ.close();

    size_t total_ops = operations.size();
    cout << "Total operations to insert: " << total_ops << endl;
    cout << endl;

    for (int percentage = 20; percentage <= 100; percentage += 20) {
        size_t ops_to_insert = (total_ops * percentage) / 100;
        
        brailIndex = new Brail();
        
        tim.start();
        for (size_t i = 0; i < ops_to_insert; i++) {
            auto& op = operations[i];
            char op_type = get<0>(op);
            Timestamp start = get<1>(op);
            Timestamp end = get<2>(op);
            Timestamp duration = end - start;
            
            brailIndex->insert(end, start, duration);
        }
        double insert_time = tim.stop();
        double avg_insertion_cost = (insert_time / ops_to_insert) * 1e6;
        
        cout << "========================================" << endl;
        cout << "Percentage: " << percentage << "%" << endl;
        cout << "Operations inserted: " << ops_to_insert << endl;
        cout << "Insertion time [secs]: " << insert_time << endl;
        cout << "Average insertion cost [μs]: " << avg_insertion_cost << endl;
        cout << "Index size [MB]: " << brailIndex->getSize() / 1e6 << endl;
        cout << "========================================" << endl;
        cout << endl;
        
        delete brailIndex;
    }
    
    return 0;
}
