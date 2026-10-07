#ifndef BRAIL_NOMAX_HPP
#define BRAIL_NOMAX_HPP

#include <vector>
#include <algorithm>
#include "../def_global.h"

// BRAIL variant with NO max duration tracking at all
// Pure B+tree structure, no pruning capability
class BrailNoMax {
public:
    vector<Timestamp> keys, values, durations; // end_time, id, duration
    size_t size;
    vector<vector<Timestamp>> levels; // Each level stores only keys (first key of block)
    Timestamp current_block_first_key;

    BrailNoMax() : size(0), current_block_first_key(0) {
        keys.reserve(200000000);
        values.reserve(200000000);
        durations.reserve(200000000);
    }

    void insert(Timestamp end_time, Timestamp id, Timestamp duration = 0) {
        keys.push_back(end_time);
        values.push_back(id);
        durations.push_back(duration);
        size++;

        if ((size - 1) % CAPACITY == 0)
            current_block_first_key = end_time;

        if (size % CAPACITY == 0) {
            add_separator(current_block_first_key);
        }
    }

    void add_separator(Timestamp key) {
        int level = 0;
        Timestamp entry = key;
        
        while (true) {
            if (level >= levels.size())
                levels.push_back({});
            
            levels[level].push_back(entry);
            
            if (levels[level].size() % CAPACITY == 0) {
                size_t start = levels[level].size() - CAPACITY;
                entry = levels[level][start]; // first key
                level++;
            } else
                break;
        }
    }

    size_t traverse_internal(Timestamp q) const {
        if (levels.empty()) 
            return 0;
        
        size_t block = 0;
        for (int lvl = levels.size() - 1; lvl >= 0; lvl--) {
            size_t start = block * CAPACITY;
            size_t end = min(start + CAPACITY, levels[lvl].size());
            
            auto it = upper_bound(levels[lvl].begin() + start, levels[lvl].begin() + end, q);
            size_t pos = it - (levels[lvl].begin() + start);
            if (pos > 0)
                block = start + pos - 1;
            else
                block = start;
        }
        return block;
    }

    size_t find_position(Timestamp q, bool lower) const {
        size_t start_blk = traverse_internal(q);
        if (lower && !levels.empty())
            while (start_blk > 0 && levels[0][start_blk] == q)
                start_blk--;
        size_t start_idx = start_blk * CAPACITY;
        auto it = lower 
            ? lower_bound(keys.begin() + start_idx, keys.begin() + size, q) 
            : upper_bound(keys.begin() + start_idx, keys.begin() + size, q);
        return it - keys.begin();
    }

    void q1(Timestamp qs, Timestamp qe, vector<Timestamp>& result) const {
        size_t first = find_position(qs, true);
        size_t last = find_position(qe, true);
        result.insert(result.end(), values.begin() + first, values.begin() + last);
    }

    void q1_workload_count(Timestamp qs, Timestamp qe, vector<Timestamp>& result, size_t &comparisons) const {
        size_t first = find_position(qs, true);
        size_t last = find_position(qe, true);
        comparisons = 0;
        result.insert(result.end(), values.begin() + first, values.begin() + last);
    }

    // Q2 - NO pruning, must scan from qe to end
    void q2(Timestamp qe, vector<Timestamp>& result) const {
        size_t first = find_position(qe, true);
        for (size_t i = first; i < size; i++) {
            if (keys[i] - qe <= durations[i])
                result.push_back(values[i]);
        }
    }

    void q2_workload_count(Timestamp qe, vector<Timestamp>& result, size_t &comparisons) const {
        size_t first = find_position(qe, true);
        for (size_t i = first; i < size; i++) {
            comparisons++;
            if (keys[i] - qe <= durations[i])
                result.push_back(values[i]);
        }
    }

    inline void scan_range(size_t first, size_t last, Timestamp qe, vector<Timestamp>& result) const {
        for (size_t i = first; i < last; i++) {
            if (keys[i] - qe <= durations[i])
                result.push_back(values[i]);
        }
    }

    size_t getSize() const {
        size_t total_size = sizeof(*this);        
        total_size += 3 * keys.size() * sizeof(Timestamp);
        total_size += levels.size() * sizeof(vector<Timestamp>);
        for (const auto& level : levels)
            total_size += level.size() * sizeof(Timestamp);
        return total_size;
    }

    size_t getHeight() const {
        return levels.size() + 1;
    }

    size_t getNumLeafBlocks() const {
        return (size + CAPACITY - 1) / CAPACITY;
    }
};

#endif