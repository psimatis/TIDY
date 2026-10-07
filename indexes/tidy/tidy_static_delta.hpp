#ifndef TIDY_STATIC_DELTA_HPP
#define TIDY_STATIC_DELTA_HPP

#include "../bplustrees/brail.hpp"
#include "../containers/relation.h"
#include "../def_global.h"
#include <vector>
#include <algorithm>

class TIDY_STATIC_DELTA {
public:
    Brail B_s, B_l;
    Timestamp delta;
    bool warmup_complete;
    bool use_oracle_delta;
    size_t warmup_size, num_outliers;
    vector<Record> warmup_buffer;
    double outlier_fraction;
    vector<Timestamp> top_k_vec;
    
    // Constructor for oracle mode (delta given)
    TIDY_STATIC_DELTA(Timestamp given_delta) 
        : delta(given_delta), warmup_complete(true), use_oracle_delta(true), 
          warmup_size(0), num_outliers(0), outlier_fraction(0) {}
    
    // Constructor for estimated mode (M and fraction given)
    TIDY_STATIC_DELTA(size_t M, double fraction) 
        : delta(0), warmup_complete(false), use_oracle_delta(false),
          warmup_size(M), outlier_fraction(fraction) {
        num_outliers = (size_t)(outlier_fraction * M);
    }
    
    void complete_warmup() {
        warmup_complete = true;
        
        if (num_outliers > 0 && top_k_vec.size() >= num_outliers) {
            nth_element(top_k_vec.begin(), top_k_vec.begin() + num_outliers - 1, top_k_vec.end(), greater<Timestamp>());
            delta = top_k_vec[num_outliers - 1];
        } else 
            delta = 0;
        
        for (const auto& r : warmup_buffer) {
            Timestamp duration = r.end - r.start;
            if (duration <= delta)
                B_s.insert(r.end, r.id, duration);
            else
                B_l.insert(r.end, r.id, duration);
        }
        
        vector<Record>().swap(warmup_buffer);
        vector<Timestamp>().swap(top_k_vec);
    }
    
    void insert(const Record& r) {
        if (!warmup_complete) {
            warmup_buffer.push_back(r);
            
            if (num_outliers > 0) {
                Timestamp dur = r.end - r.start;
                top_k_vec.push_back(dur);
            }
            
            if (warmup_buffer.size() >= warmup_size)
                complete_warmup();
        } else {
            Timestamp duration = r.end - r.start;
            if (duration <= delta)
                B_s.insert(r.end, r.id, duration);
            else
                B_l.insert(r.end, r.id, duration);
        }
    }
    
    void query(Timestamp qs, Timestamp qe, vector<RecordId>& results) const {
        B_s.q2_no_up(qe, results);
        B_l.q2_no_up(qe, results);

        B_l.q1(qs, qe, results);
        B_s.q1(qs, qe, results);

        if (!warmup_complete && !warmup_buffer.empty()) {
            for (const auto& rec : warmup_buffer) {
                Timestamp dur = rec.end - rec.start;
                if (rec.end >= qs && rec.end < qe) 
                    results.push_back(rec.id);
                else if (rec.end >= qe && dur >= rec.end - qe)
                    results.push_back(rec.id);
            }
        }
    }
   
    void stab_query(Timestamp q, vector<RecordId>& results) const {
        B_s.q2_no_up(q, results);
        
        B_l.q2_no_up(q, results);
        
        if (!warmup_complete && !warmup_buffer.empty()) {
            for (const auto& rec : warmup_buffer) {
                Timestamp dur = rec.end - rec.start;
                if (rec.end >= q && dur >= rec.end - q)
                    results.push_back(rec.id);
            }
        }
    }

    size_t short_count() const { return B_s.size; }
    size_t long_count() const { return B_l.size; }
    Timestamp get_delta() const { return delta; }
    bool is_oracle_mode() const { return use_oracle_delta; }
    size_t getSize() const {return B_s.getSize() + B_l.getSize();}
    size_t getShortHeight() const { return B_s.getHeight(); }
    size_t getLongHeight() const { return B_l.getHeight(); }
    size_t getShortLeafBlocks() const { return B_s.getNumLeafBlocks(); }
    size_t getLongLeafBlocks() const { return B_l.getNumLeafBlocks(); }
};

#endif
