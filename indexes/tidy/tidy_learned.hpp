#ifndef TIDY_LEARNED_HPP
#define TIDY_LEARNED_HPP

#include "../bplustrees/brail.hpp"
#include "../containers/relation.h"
#include "../def_global.h"
#include <vector>
#include <algorithm>
#include <queue>

class TIDY_LEARNED {
public:
    Brail B_s, B_l;
    double rho, delta = 0, learning_rate;
    bool warmup_complete = false;
    size_t warmup_size, num_outliers, incr, decr;
    vector<Record> warmup_buffer;
    priority_queue<Timestamp, vector<Timestamp>, greater<Timestamp>> top_k;

    TIDY_LEARNED(double rho_val, double lr, size_t M)
        : rho(rho_val), learning_rate(lr), warmup_size(M) {
        warmup_buffer.reserve(M);
        num_outliers = (size_t)(rho * M);
        incr = learning_rate * (1.0 - rho);
        decr = learning_rate * rho;
    }
    
    void complete_warmup() {
        warmup_complete = true;
        delta = top_k.top();
        
        for (const auto& r : warmup_buffer) {
            Timestamp dur = r.end - r.start;
            if (dur <= delta)
                B_s.insert(r.end, r.id, dur);
            else
                B_l.insert(r.end, r.id, dur);
        }
    }
    
    void insert(const Record& r) {
        Timestamp dur = r.end - r.start;
        
        if (!warmup_complete) {
            warmup_buffer.push_back(r);
            if (num_outliers > 0) {
                if (top_k.size() < num_outliers)
                    top_k.push(dur);
                else if (dur > top_k.top()) {
                    top_k.pop();
                    top_k.push(dur);
                }
            }
            if (warmup_buffer.size() >= warmup_size)
                complete_warmup();
        } else {
            if (dur > delta) {
                delta += incr;
                B_l.insert(r.end, r.id, dur);
            }
            else {
                delta -= decr;
                B_s.insert(r.end, r.id, dur);
            }            
        }
    }
    
    void query(Timestamp qs, Timestamp qe, vector<RecordId>& results) const {
        if (!warmup_complete) {
            for (const auto& r : warmup_buffer) {
                if (r.start <= qe && r.end >= qs)
                    results.push_back(r.id);
            }
            return;
        }

        size_t s_pos = B_s.q2_no_up(qe, results);
        size_t l_pos = B_l.q2_no_up(qe, results);

        B_l.q1(qs, l_pos, results);
        B_s.q1(qs, s_pos, results);
    }

    void query_opt(Timestamp qs, Timestamp qe, vector<RecordId>& results) const {
        if (!warmup_complete) {
            for (const auto& r : warmup_buffer) {
                if (r.start <= qe && r.end >= qs)
                    results.push_back(r.id);
            }
            return;
        }

        size_t s_last = B_s.q2_no_up(qe, results);
        size_t l_last = B_l.q2_no_up(qe, results);

        size_t s_first = B_s.find_position(qs, true);
        size_t l_first = B_l.find_position(qs, true);

        results.reserve(results.size() + (s_last - s_first) + (l_last - l_first));
        results.insert(results.end(), B_l.values.begin() + l_first, B_l.values.begin() + l_last);
        results.insert(results.end(), B_s.values.begin() + s_first, B_s.values.begin() + s_last);
    }

    void stab_query(Timestamp q, vector<RecordId>& results) const {
        if (!warmup_complete) {
            for (const auto& r : warmup_buffer) {
                if (r.start <= q && r.end >= q)
                    results.push_back(r.id);
            }
            return;
        }
        
        B_s.q2_no_up(q, results);
        
        B_l.q2_no_up(q, results);
    }
    
    size_t short_count() const { return B_s.size; }
    size_t long_count() const { return B_l.size; }
    Timestamp get_delta() const { return (Timestamp)delta; }
    double get_learning_rate() const { return learning_rate; }
    size_t get_warmup_size() const { return warmup_size; }
    size_t getSize() const {return B_s.getSize() + B_l.getSize();}
    size_t getShortHeight() const { return B_s.getHeight(); }
    size_t getLongHeight() const { return B_l.getHeight(); }
    size_t getShortLeafBlocks() const { return B_s.getNumLeafBlocks(); }
    size_t getLongLeafBlocks() const { return B_l.getNumLeafBlocks(); }
};

#endif
