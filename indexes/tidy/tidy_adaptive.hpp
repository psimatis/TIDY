#ifndef TIDY_ADAPTIVE_HPP
#define TIDY_ADAPTIVE_HPP

#include "../bplustrees/brail.hpp"
#include "../containers/relation.h"
#include "../def_global.h"
#include <vector>
#include <algorithm>

using namespace std;

class TIDY_ADAPTIVE {
public:
    Brail B_s, B_l;
    double rho;
    size_t buffer_capacity, pq_size;
    vector<Record> buf;
    vector<Record> pq;
    vector<Record> aponera;
    
    TIDY_ADAPTIVE(double rho_val) : rho(rho_val) {
        buffer_capacity = (size_t)(CAPACITY / (1.0 - rho));
        buf.reserve(buffer_capacity + 1);
        pq_size = buffer_capacity - CAPACITY;
        pq.reserve(pq_size);
        aponera.reserve(CAPACITY);
    }
    
    void insert(const Record& r) {
        if (pq.size() < pq_size) {
            pq.push_back(r);
            if (pq.size() == pq_size)
                make_heap(pq.begin(), pq.end(), [](const Record& a, const Record& b) {return (a.end - a.start) < (b.end - b.start);});
        }
        else {
            if (pq.front().end - pq.front().start < r.end - r.start) {
                aponera.push_back(pq.front());
                pop_heap(pq.begin(), pq.end(), [](const Record& a, const Record& b) {return (a.end - a.start) < (b.end - b.start);}); 
                pq.pop_back();
                pq.push_back(r); 
                push_heap(pq.begin(),pq.end(), [](const Record& a, const Record& b) {return (a.end - a.start) < (b.end - b.start);});             
            }
            else { 
                buf.push_back(r);
            }
        }

        if (buf.size() + pq.size() + aponera.size() >= buffer_capacity)
            finalize_leaf();
    }

    void finalize_leaf() {
        sort(pq.begin(), pq.end(), CompareByEnd);
        sort(aponera.begin(), aponera.end(), CompareByEnd);

        size_t bi = 0, ai = 0;
        const size_t b_n = buf.size();
        const size_t a_n = aponera.size();

        while (bi < b_n || ai < a_n) {
            if ((ai >= a_n) || (bi < b_n && buf[bi].end <= aponera[ai].end)) {
                B_s.insert(buf[bi].end, buf[bi].id, buf[bi].end - buf[bi].start);
                bi++;
            } else {
                B_s.insert(aponera[ai].end, aponera[ai].id, aponera[ai].end - aponera[ai].start);
                ai++;
            }
        }
        
        for (size_t i = 0; i < pq.size(); i++) 
            B_l.insert(pq[i].end, pq[i].id, pq[i].end - pq[i].start);
        
        buf.clear();
        pq.clear();
        aponera.clear();
    }
    
    void query(Timestamp qs, Timestamp qe, vector<RecordId>& results) const {
        B_s.q2_no_up(qe, results);
        B_l.q2_no_up(qe, results);

        B_l.q1(qs, qe, results);
        B_s.q1(qs, qe, results);
        
        for (const auto& rec : buf) {
            Timestamp dur = rec.end - rec.start;
            if (rec.end >= qs && rec.end < qe) 
                results.push_back(rec.id);
            else if (rec.end >= qe && dur >= rec.end - qe)
                results.push_back(rec.id);
        }
        for (const auto& rec : pq) {
            Timestamp dur = rec.end - rec.start;
            if (rec.end >= qs && rec.end < qe) 
                results.push_back(rec.id);
            else if (rec.end >= qe && dur >= rec.end - qe)
                results.push_back(rec.id);
        }
        for (const auto& rec : aponera) {
            Timestamp dur = rec.end - rec.start;
            if (rec.end >= qs && rec.end < qe) 
                results.push_back(rec.id);
            else if (rec.end >= qe && dur >= rec.end - qe)
                results.push_back(rec.id);
        }
    }
    
    void stab_query(Timestamp q, vector<RecordId>& results) const {
        B_s.q2_no_up(q, results);
        
        B_l.q2_no_up(q, results);
        
        for (const auto& rec : buf) {
            Timestamp dur = rec.end - rec.start;
            if (rec.end >= q && dur >= rec.end - q)
                results.push_back(rec.id);
        }
        for (const auto& rec : pq) {
            Timestamp dur = rec.end - rec.start;
            if (rec.end >= q && dur >= rec.end - q)
                results.push_back(rec.id);
        }
        for (const auto& rec : aponera) { // sorted by end time
            Timestamp dur = rec.end - rec.start;
            if (rec.end >= q && dur >= rec.end - q)
                results.push_back(rec.id);
        }
    }
    
    size_t short_count() const { return B_s.size; }
    size_t long_count() const { return B_l.size; }
    
    size_t getSize() const {
        return B_s.getSize() + B_l.getSize();
    }

    size_t getShortHeight() const { return B_s.getHeight(); }
    size_t getLongHeight() const { return B_l.getHeight(); }
    size_t getShortLeafBlocks() const { return B_s.getNumLeafBlocks(); }
    size_t getLongLeafBlocks() const { return B_l.getNumLeafBlocks(); }
};

#endif