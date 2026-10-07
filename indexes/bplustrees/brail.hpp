#ifndef BRAIL_HPP
#define BRAIL_HPP

#include <vector>
#include <algorithm>
#include "../def_global.h"

// Stores first key of block and max duration in subtree
struct SeparatorEntryFirst {
    Timestamp key;
    Timestamp maxd;
};

class Brail {
public:
    vector<Timestamp> keys, values, durations; // end_time, id, duration
    size_t size;
    vector<vector<SeparatorEntryFirst>> levels; // Each level stores separator entries (key, maxd)
    Timestamp current_block_maxd; // Tracks block's max duration during construction
    Timestamp current_block_first_key; // Tracks first key of block during construction
    Timestamp maxdur; // Global max duration for B_l optimization

    Brail() : size(0), current_block_maxd(0), current_block_first_key(0), maxdur(0) {
        keys.reserve(200000000);
        values.reserve(200000000);
        durations.reserve(200000000);
    }

    void insert(Timestamp end_time, Timestamp id, Timestamp duration = 0) {
        keys.push_back(end_time);
        values.push_back(id);
        durations.push_back(duration);
        size++;

        current_block_maxd = max(current_block_maxd, duration);
        maxdur = max(maxdur, duration);

        if ((size - 1) % CAPACITY == 0)
            current_block_first_key = end_time;

        // Every CAPACITY elements, finalize block and add separator
        if (size % CAPACITY == 0) {
            add_separator(current_block_first_key, current_block_maxd);
            current_block_maxd = 0;
        }
    }

    void add_separator(Timestamp key, Timestamp maxd) {
        int level = 0;
        SeparatorEntryFirst entry = {key, maxd};
        
        while (true) {
            if (level >= levels.size())
                levels.push_back({});
            
            levels[level].push_back(entry);
            
            // Propagate up every CAPACITY separators
            if (levels[level].size() % CAPACITY == 0) {
                Timestamp upper_maxd = 0;
                size_t start = levels[level].size() - CAPACITY;
                for (size_t i = start; i < levels[level].size(); i++) 
                    upper_maxd = max(upper_maxd, levels[level][i].maxd);
                entry = {levels[level][start].key, upper_maxd}; // first key, max dur
                level++;
            } else
                break;
        }
    }

    // Find the leaf block containing q (i.e., the last block whose first key <= q)
    size_t traverse_internal(Timestamp q) const {
        if (levels.empty()) 
            return 0;
        
        size_t block = 0;
        for (int lvl = levels.size() - 1; lvl >= 0; lvl--) {
            size_t start = block * CAPACITY;
            size_t end = min(start + CAPACITY, levels[lvl].size());
            
            auto it = upper_bound(levels[lvl].begin() + start, levels[lvl].begin() + end, q,
                    [](Timestamp val, const SeparatorEntryFirst& e) { return val < e.key; });
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
            while (start_blk > 0 && levels[0][start_blk].key == q)
                start_blk--;
        size_t start_idx = start_blk * CAPACITY;
        auto it = lower 
            ? lower_bound(keys.begin() + start_idx, keys.begin() + size, q) 
            : upper_bound(keys.begin() + start_idx, keys.begin() + size, q);
        return it - keys.begin();
    }

    // Q1: all records where qs <= t_e < qe
    void q1(Timestamp qs, Timestamp qe, vector<Timestamp>& result) const {
        size_t first = find_position(qs, true);
        size_t last = find_position(qe, true);
        result.insert(result.end(), values.begin() + first, values.begin() + last);
    }

    // Q1 overload: accepts a precomputed last position (avoids redundant find_position(qe))
    void q1(Timestamp qs, size_t last, vector<Timestamp>& result) const {
        size_t first = find_position(qs, true);
        result.insert(result.end(), values.begin() + first, values.begin() + last);
    }

    void q1_workload_count(Timestamp qs, Timestamp qe, vector<Timestamp>& result, size_t &comparisons) const {
        size_t first = find_position(qs, true);
        size_t last = find_position(qe, true);

        comparisons = 0;
        result.insert(result.end(), values.begin() + first, values.begin() + last);
    
    }

    // Number of leaf blocks covered by one separator at the given level (CAPACITY^level)
    static size_t blocks_per_separator(int level) {
        size_t n = 1;
        for (int i = 0; i < level; i++)
            n *= CAPACITY;
        return n;
    }

    size_t q2_no_up(Timestamp qe, vector<Timestamp>& result) const {
        size_t first = find_position(qe, true);
        size_t blk = first / CAPACITY;
        
        while (blk < getNumLeafBlocks()) {
            size_t scan_start = max(blk * CAPACITY, first);
            
            if (scan_start < size && keys[scan_start] - qe > maxdur)
                break;
            
            size_t skip = 0;
            for (int level = levels.size() - 1; level >= 0; level--) {
                size_t group_size = blocks_per_separator(level);
                if (blk % group_size != 0) 
                    continue;

                size_t sep_idx = blk / group_size;
                if (sep_idx >= levels[level].size()) 
                    continue;

                if (keys[scan_start] - qe > levels[level][sep_idx].maxd) {
                    skip = group_size;
                    break;
                }
            }
            if (skip) { 
                blk += skip; 
                continue; 
            }
            size_t scan_end = min(blk * CAPACITY + CAPACITY, size);
            scan_range(scan_start, scan_end, qe, result);
            blk++;
        }
        return first;
    }

    void q2_no_up_workload_count(Timestamp qe, vector<Timestamp>& result, size_t &comparisons) const {
        size_t first = find_position(qe, true);
        size_t blk = first / CAPACITY;
        
        while (blk < getNumLeafBlocks()) {
            size_t scan_start = max(blk * CAPACITY, first);
            
            if (scan_start < size && keys[scan_start] - qe > maxdur)
                break;
            
            size_t skip = 0;
            for (int level = levels.size() - 1; level >= 0; level--) {
                size_t group_size = blocks_per_separator(level);
                if (blk % group_size != 0) 
                    continue;

                size_t sep_idx = blk / group_size;
                if (sep_idx >= levels[level].size()) 
                    continue;

                if (keys[scan_start] - qe > levels[level][sep_idx].maxd) {
                    skip = group_size;
                    break;
                }
            }
            if (skip) { 
                blk += skip; 
                continue; 
            }
            size_t scan_end = min(blk * CAPACITY + CAPACITY, size);
            for (size_t i = scan_start; i < scan_end; i++) {
                comparisons++;
                if (keys[i] - qe <= durations[i])
                    result.push_back(values[i]);
            }
            blk++;
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
        total_size += levels.size() * sizeof(vector<SeparatorEntryFirst>);
        for (const auto& level : levels)
            total_size += level.size() * sizeof(SeparatorEntryFirst);
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
