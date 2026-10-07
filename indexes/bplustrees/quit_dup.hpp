#ifndef BPLUS_TREE_QIT_HPP
#define BPLUS_TREE_QIT_HPP

// B+ tree based on QIT-quit. Optimizations:
// - LOL_FAT: Fast-path optimization for near-sorted data
// - REDISTRIBUTE: Load balancing between adjacent leaves
// - VARIABLE_SPLIT: IKR-based split position calculation
// - LOL_RESET: Reset fast-path after consecutive failures
// Modified to support duplicate keys (appends values instead of overwriting)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

using namespace std;

static constexpr uint32_t BLOCK_SIZE_BYTES = 12288;
static constexpr uint32_t MAX_DEPTH = 10;

// Outlier detection
namespace IKR {
    inline size_t upper_bound(size_t dq, uint16_t n1, uint16_t n2) {
        return (dq / 0.7) * n2 / n1;
    }
}

// LOL_RESET: Track consecutive fast-path failures
struct reset_stats {
    uint8_t fails;
    uint8_t threshold;

    explicit reset_stats(uint8_t t) : fails(0), threshold(t) {}

    void success() { fails = 0; }

    bool failure() {
        fails++;
        return fails >= threshold;
    }

    void reset() { fails = 0; }
};

// Contiguous block memory manager
class BlockManager {
    struct Block {
        uint8_t data[BLOCK_SIZE_BYTES]{};
    };
    
    uint32_t next_id_ = 0;
    vector<Block> blocks_;
    
public:
    BlockManager(uint32_t capacity = 1000000) {
        blocks_.reserve(capacity);
    }
    
    uint32_t allocate() {
        blocks_.emplace_back();
        return next_id_++;
    }
    
    void* open_block(uint32_t id) {
        return blocks_[id].data;
    }
    
    size_t getSize() const {
        return sizeof(*this) + blocks_.size() * sizeof(Block);
    }
};

template<typename KeyT, typename ValueT>
class BPlusTreeQIT {
    static constexpr uint32_t INVALID_NODE_ID = static_cast<uint32_t>(-1);
    
    struct NodeInfo {
        uint32_t id;
        uint32_t next_id;
        uint16_t size;
        uint16_t type; // 0 = LEAF, 1 = INTERNAL
    };
    
    // Calculate capacities based on block size
    // Leaf stores: keys[], offsets[], counts[]
    static constexpr uint16_t LEAF_CAPACITY = 
        (BLOCK_SIZE_BYTES - sizeof(NodeInfo)) / (sizeof(KeyT) + sizeof(uint32_t) + sizeof(uint32_t));
    static constexpr uint16_t INTERNAL_CAPACITY = 
        (BLOCK_SIZE_BYTES - sizeof(NodeInfo) - sizeof(uint32_t)) / (sizeof(KeyT) + sizeof(uint32_t));
    static constexpr uint16_t SPLIT_LEAF_POS = (LEAF_CAPACITY + 1) / 2;
    static constexpr uint16_t SPLIT_INTERNAL_POS = INTERNAL_CAPACITY / 2;
    static constexpr uint16_t IQR_SIZE_THRESH = SPLIT_LEAF_POS;
    
    // Node view - interprets raw block memory
    struct Node {
        NodeInfo* info;
        KeyT* keys;
        union {
            uint32_t* children;    // for internal nodes
            uint32_t* offsets;     // for leaf nodes - index into values_
        };
        uint32_t* counts;          // for leaf nodes - count per key
        
        void load(void* buf) {
            info = static_cast<NodeInfo*>(buf);
            if (info->type == 0) { // LEAF
                keys = reinterpret_cast<KeyT*>(info + 1);
                offsets = reinterpret_cast<uint32_t*>(keys + LEAF_CAPACITY);
                counts = reinterpret_cast<uint32_t*>(offsets + LEAF_CAPACITY);
            } else { // INTERNAL
                keys = reinterpret_cast<KeyT*>(info + 1);
                children = reinterpret_cast<uint32_t*>(keys + INTERNAL_CAPACITY);
            }
        }
        
        void init_leaf(void* buf) {
            info = static_cast<NodeInfo*>(buf);
            info->type = 0;
            info->size = 0;
            keys = reinterpret_cast<KeyT*>(info + 1);
            offsets = reinterpret_cast<uint32_t*>(keys + LEAF_CAPACITY);
            counts = reinterpret_cast<uint32_t*>(offsets + LEAF_CAPACITY);
        }
        
        void init_internal(void* buf) {
            info = static_cast<NodeInfo*>(buf);
            info->type = 1;
            info->size = 0;
            keys = reinterpret_cast<KeyT*>(info + 1);
            children = reinterpret_cast<uint32_t*>(keys + INTERNAL_CAPACITY);
        }
        
        void to_internal(void* buf) {
            info = static_cast<NodeInfo*>(buf);
            info->type = 1;
            keys = reinterpret_cast<KeyT*>(info + 1);
            children = reinterpret_cast<uint32_t*>(keys + INTERNAL_CAPACITY);
        }
        
        uint16_t value_slot(const KeyT& key) const {
            return lower_bound(keys, keys + info->size, key) - keys;
        }
        
        uint16_t value_slot_upper(const KeyT& key) const {
            return upper_bound(keys, keys + info->size, key) - keys;
        }
        
        uint16_t child_slot(const KeyT& key) const {
            return upper_bound(keys, keys + info->size, key) - keys;
        }
    };
    
    using path_t = array<uint32_t, MAX_DEPTH>;
    using dist_f = size_t (*)(const KeyT&, const KeyT&);
    
    BlockManager manager_;
    uint32_t root_id_;
    uint32_t head_id_;
    uint32_t tail_id_;
    uint8_t depth_;
    size_t size_;
    size_t unique_keys_;
    
    // Contiguous value storage (for duplicate support)
    vector<ValueT> values_;
    
    // LOL_FAT fast-path state
    uint32_t fp_id_;
    KeyT fp_min_;
    KeyT fp_max_;
    path_t fp_path_;
    uint32_t lol_prev_id_;
    KeyT lol_prev_min_;
    uint16_t lol_prev_size_;
    uint16_t lol_size_;
    
    // LOL_RESET state
    reset_stats life_;
    
    static size_t dist_func(const KeyT& max, const KeyT& min) { return max - min; }
    dist_f dist_ = dist_func;
    
    KeyT find_leaf(Node& node, path_t& path, const KeyT& key) {
        KeyT leaf_max = {};
        uint32_t child_id = root_id_;
        for (uint8_t i = depth_ - 1; i > 0; --i) {
            path[i] = child_id;
            node.load(manager_.open_block(child_id));
            uint16_t slot = node.child_slot(key);
            if (slot != node.info->size)
                leaf_max = node.keys[slot];
            child_id = node.children[slot];
        }
        path[0] = child_id;
        node.load(manager_.open_block(child_id));
        return leaf_max;
    }
    
    void create_new_root(const KeyT& key, uint32_t node_id) {
        uint32_t left_node_id = manager_.allocate();
        Node root, left_node;
        root.load(manager_.open_block(root_id_));
        left_node.load(manager_.open_block(left_node_id));
        memcpy(left_node.info, root.info, BLOCK_SIZE_BYTES);
        left_node.info->id = left_node_id;
        
        if (root.info->type == 0) // was leaf
            root.to_internal(manager_.open_block(root_id_));
        root.info->size = 1;
        root.keys[0] = key;
        root.children[0] = left_node_id;
        root.children[1] = node_id;
        
        if (root_id_ == head_id_)
            head_id_ = left_node_id;
        
        // Update fast-path
        if (fp_path_[depth_ - 1] == root_id_) {
            if (fp_id_ == root_id_)
                fp_id_ = left_node_id;
            fp_path_[depth_ - 1] = left_node_id;
        }
        fp_path_[depth_] = root_id_;
        depth_++;
    }
    
    // REDISTRIBUTE: Update internal node key when leaf min changes
    void update_internal(const path_t& path, const KeyT& old_key, const KeyT& new_key) {
        Node node;
        for (uint8_t i = 1; i < depth_; i++) {
            uint32_t node_id = path[i];
            node.load(manager_.open_block(node_id));
            uint16_t index = node.child_slot(old_key) - 1;
            if (index < node.info->size && node.keys[index] == old_key) {
                node.keys[index] = new_key;
                return;
            }
        }
    }
    
    void internal_insert(const path_t& path, KeyT key, uint32_t child_id, uint16_t split_pos) {
        Node node;
        for (uint8_t i = 1; i < depth_; i++) {
            uint32_t node_id = path[i];
            node.load(manager_.open_block(node_id));
            uint16_t index = node.child_slot(key);
            
            if (node.info->size < INTERNAL_CAPACITY) {
                memmove(node.keys + index + 1, node.keys + index, (node.info->size - index) * sizeof(KeyT));
                memmove(node.children + index + 2, node.children + index + 1, (node.info->size - index) * sizeof(uint32_t));
                node.keys[index] = key;
                node.children[index + 1] = child_id;
                ++node.info->size;
                return;
            }
            
            // Split internal node
            uint32_t new_node_id = manager_.allocate();
            Node new_node;
            new_node.init_internal(manager_.open_block(new_node_id));
            
            node.info->size = split_pos;
            new_node.info->id = new_node_id;
            new_node.info->size = INTERNAL_CAPACITY - node.info->size;
            
            if (index < node.info->size) {
                memcpy(new_node.keys, node.keys + node.info->size, new_node.info->size * sizeof(KeyT));
                memmove(node.keys + index + 1, node.keys + index, (node.info->size - index) * sizeof(KeyT));
                node.keys[index] = key;
                memcpy(new_node.children, node.children + node.info->size, (new_node.info->size + 1) * sizeof(uint32_t));
                memmove(node.children + index + 2, node.children + index + 1, (node.info->size - index + 1) * sizeof(uint32_t));
                node.children[index + 1] = child_id;
                key = node.keys[node.info->size];
            } else if (index == node.info->size) {
                memcpy(new_node.keys, node.keys + node.info->size, new_node.info->size * sizeof(KeyT));
                memcpy(new_node.children + 1, node.children + 1 + node.info->size, new_node.info->size * sizeof(uint32_t));
                new_node.children[0] = child_id;
            } else {
                memcpy(new_node.keys, node.keys + node.info->size + 1, (index - node.info->size - 1) * sizeof(KeyT));
                memcpy(new_node.keys + index - node.info->size, node.keys + index, (INTERNAL_CAPACITY - index) * sizeof(KeyT));
                new_node.keys[index - node.info->size - 1] = key;
                memcpy(new_node.children, node.children + 1 + node.info->size, (index - node.info->size) * sizeof(uint32_t));
                memcpy(new_node.children + 1 + index - node.info->size, node.children + 1 + index, new_node.info->size * sizeof(uint32_t));
                new_node.children[index - node.info->size] = child_id;
                key = node.keys[node.info->size];
            }
            
            // Update fast-path
            if (fp_path_[i] == node_id && fp_id_ != head_id_ && key <= fp_min_)
                fp_path_[i] = new_node_id;
            child_id = new_node_id;
        }
        create_new_root(key, child_id);
    }
    
    // REDISTRIBUTE: Move entries from current leaf to lol_prev to balance load
    // This is called when lol_prev_size < IQR_SIZE_THRESH
    void redistribute(Node& leaf, uint16_t index, const KeyT& key, const ValueT& value) {
        // Move values from leaf to lol_prev
        uint16_t items = IQR_SIZE_THRESH - lol_prev_size_;  // items to move to lol_prev
        
        Node lol_prev;
        lol_prev.load(manager_.open_block(lol_prev_id_));
        
        if (index < items) {
            // New key goes into lol_prev
            --items;
            memcpy(lol_prev.keys + lol_prev_size_, leaf.keys, index * sizeof(KeyT));
            memcpy(lol_prev.keys + lol_prev_size_ + index + 1, leaf.keys + index, (items - index) * sizeof(KeyT));
            lol_prev.keys[lol_prev_size_ + index] = key;
            
            memcpy(lol_prev.offsets + lol_prev_size_, leaf.offsets, index * sizeof(uint32_t));
            memcpy(lol_prev.offsets + lol_prev_size_ + index + 1, leaf.offsets + index, (items - index) * sizeof(uint32_t));
            lol_prev.offsets[lol_prev_size_ + index] = values_.size();
            
            memcpy(lol_prev.counts + lol_prev_size_, leaf.counts, index * sizeof(uint32_t));
            memcpy(lol_prev.counts + lol_prev_size_ + index + 1, leaf.counts + index, (items - index) * sizeof(uint32_t));
            lol_prev.counts[lol_prev_size_ + index] = 1;
            values_.push_back(value);
            
            memmove(leaf.keys, leaf.keys + items, (lol_size_ - items) * sizeof(KeyT));
            memmove(leaf.offsets, leaf.offsets + items, (lol_size_ - items) * sizeof(uint32_t));
            memmove(leaf.counts, leaf.counts + items, (lol_size_ - items) * sizeof(uint32_t));
            ++items;
        } else {
            memcpy(lol_prev.keys + lol_prev_size_, leaf.keys, items * sizeof(KeyT));
            memcpy(lol_prev.offsets + lol_prev_size_, leaf.offsets, items * sizeof(uint32_t));
            memcpy(lol_prev.counts + lol_prev_size_, leaf.counts, items * sizeof(uint32_t));
            
            // Move leaf entries left and insert new key
            uint16_t new_index = index - items;
            memmove(leaf.keys, leaf.keys + items, new_index * sizeof(KeyT));
            memmove(leaf.keys + new_index + 1, leaf.keys + index, (lol_size_ - index) * sizeof(KeyT));
            leaf.keys[new_index] = key;
            
            memmove(leaf.offsets, leaf.offsets + items, new_index * sizeof(uint32_t));
            memmove(leaf.offsets + new_index + 1, leaf.offsets + index, (lol_size_ - index) * sizeof(uint32_t));
            leaf.offsets[new_index] = values_.size();
            
            memmove(leaf.counts, leaf.counts + items, new_index * sizeof(uint32_t));
            memmove(leaf.counts + new_index + 1, leaf.counts + index, (lol_size_ - index) * sizeof(uint32_t));
            leaf.counts[new_index] = 1;
            values_.push_back(value);
        }
        
        // Update parent for current leaf min
        update_internal(fp_path_, fp_min_, leaf.keys[0]);
        
        // Update state
        fp_min_ = leaf.keys[0];
        lol_size_ = lol_size_ - items + 1;
        lol_prev_size_ = IQR_SIZE_THRESH;
        leaf.info->size = lol_size_;
        lol_prev.info->size = IQR_SIZE_THRESH;
        
        unique_keys_++;
        size_++;
    }
    
    bool leaf_insert(Node& leaf, const path_t& path, const KeyT& key, const ValueT& value) {
        uint16_t index = leaf.value_slot(key);
        
        // DUPLICATE SUPPORT: append value instead of overwriting
        if (index < leaf.info->size && leaf.keys[index] == key) {
            values_.push_back(value);
            leaf.counts[index]++;
            size_++;
            return true;
        }
        
        // New key
        unique_keys_++;
        size_++;
        
        if (leaf.info->size < LEAF_CAPACITY) {
            memmove(leaf.keys + index + 1, leaf.keys + index, (leaf.info->size - index) * sizeof(KeyT));
            memmove(leaf.offsets + index + 1, leaf.offsets + index, (leaf.info->size - index) * sizeof(uint32_t));
            memmove(leaf.counts + index + 1, leaf.counts + index, (leaf.info->size - index) * sizeof(uint32_t));
            leaf.keys[index] = key;
            leaf.offsets[index] = values_.size();
            leaf.counts[index] = 1;
            values_.push_back(value);
            ++leaf.info->size;
            
            // LOL_FAT bookkeeping
            if (leaf.info->id == fp_id_)
                lol_size_++;
            else if (leaf.info->next_id == fp_id_) {
                lol_prev_id_ = leaf.info->id;
                lol_prev_min_ = leaf.keys[0];
                lol_prev_size_ = leaf.info->size;
            }
            return true;
        }
        
        // Need to split - undo the size increments (will be re-added after)
        unique_keys_--;
        size_--;
        
        // VARIABLE_SPLIT: Use IQR-based split position
        uint16_t split_leaf_pos = SPLIT_LEAF_POS;
        bool lol_move = false;
        
        if (leaf.info->id == fp_id_) {
            if (lol_prev_id_ == INVALID_NODE_ID)
                lol_move = true;  // move from head
            else if (lol_prev_size_ >= IQR_SIZE_THRESH) {
                // IQR has enough information
                size_t max_distance = IKR::upper_bound(
                    dist_(fp_min_, lol_prev_min_), lol_prev_size_, lol_size_);
                uint16_t outlier_pos = leaf.value_slot_upper(fp_min_ + max_distance);
                if (outlier_pos <= SPLIT_LEAF_POS)
                    split_leaf_pos = outlier_pos;
                else {
                    if (outlier_pos - 10 < SPLIT_LEAF_POS)
                        split_leaf_pos = SPLIT_LEAF_POS;
                    else
                        split_leaf_pos = outlier_pos - 10;
                    lol_move = true;
                }
                if (index < outlier_pos)
                    split_leaf_pos++;
            } else {
                // REDISTRIBUTE: lol_prev doesn't have enough entries
                redistribute(leaf, index, key, value);
                return true;
            }
        }
        
        // Re-add the size increments
        unique_keys_++;
        size_++;
        
        // Allocate new leaf
        uint32_t new_leaf_id = manager_.allocate();
        Node new_leaf;
        new_leaf.init_leaf(manager_.open_block(new_leaf_id));
        
        leaf.info->size = split_leaf_pos;
        new_leaf.info->id = new_leaf_id;
        new_leaf.info->next_id = leaf.info->next_id;
        leaf.info->next_id = new_leaf_id;
        new_leaf.info->size = LEAF_CAPACITY + 1 - leaf.info->size;
        
        if (index < leaf.info->size) {
            memcpy(new_leaf.keys, leaf.keys + leaf.info->size - 1, new_leaf.info->size * sizeof(KeyT));
            memmove(leaf.keys + index + 1, leaf.keys + index, (leaf.info->size - index - 1) * sizeof(KeyT));
            leaf.keys[index] = key;
            
            memcpy(new_leaf.offsets, leaf.offsets + leaf.info->size - 1, new_leaf.info->size * sizeof(uint32_t));
            memmove(leaf.offsets + index + 1, leaf.offsets + index, (leaf.info->size - index - 1) * sizeof(uint32_t));
            leaf.offsets[index] = values_.size();
            
            memcpy(new_leaf.counts, leaf.counts + leaf.info->size - 1, new_leaf.info->size * sizeof(uint32_t));
            memmove(leaf.counts + index + 1, leaf.counts + index, (leaf.info->size - index - 1) * sizeof(uint32_t));
            leaf.counts[index] = 1;
            values_.push_back(value);
        } else {
            uint16_t new_index = index - leaf.info->size;
            memcpy(new_leaf.keys, leaf.keys + leaf.info->size, new_index * sizeof(KeyT));
            new_leaf.keys[new_index] = key;
            memcpy(new_leaf.keys + new_index + 1, leaf.keys + index, (LEAF_CAPACITY - index) * sizeof(KeyT));
            
            memcpy(new_leaf.offsets, leaf.offsets + leaf.info->size, new_index * sizeof(uint32_t));
            new_leaf.offsets[new_index] = values_.size();
            memcpy(new_leaf.offsets + new_index + 1, leaf.offsets + index, (LEAF_CAPACITY - index) * sizeof(uint32_t));
            
            memcpy(new_leaf.counts, leaf.counts + leaf.info->size, new_index * sizeof(uint32_t));
            new_leaf.counts[new_index] = 1;
            memcpy(new_leaf.counts + new_index + 1, leaf.counts + index, (LEAF_CAPACITY - index) * sizeof(uint32_t));
            values_.push_back(value);
        }
        
        if (leaf.info->id == tail_id_)
            tail_id_ = new_leaf_id;
        
        // LOL_FAT: update fast-path after split
        if (leaf.info->id == fp_id_) {
            if (lol_move) {
                lol_prev_min_ = fp_min_;
                lol_prev_size_ = leaf.info->size;
                lol_prev_id_ = fp_id_;
                fp_id_ = new_leaf_id;
                fp_min_ = new_leaf.keys[0];
                lol_size_ = new_leaf.info->size;
                fp_path_[0] = fp_id_;
            } else {
                fp_max_ = new_leaf.keys[0];
                lol_size_ = leaf.info->size;
            }
        } else if (new_leaf.info->next_id == fp_id_) {
            lol_prev_id_ = new_leaf_id;
            lol_prev_min_ = new_leaf.keys[0];
            lol_prev_size_ = new_leaf.info->size;
        }
        
        internal_insert(path, new_leaf.keys[0], new_leaf_id, SPLIT_INTERNAL_POS);
        return true;
    }
    
public:
    BPlusTreeQIT() : life_(static_cast<uint8_t>(sqrt(LEAF_CAPACITY))) {
        values_.reserve(10000000);
        root_id_ = manager_.allocate();
        head_id_ = tail_id_ = root_id_;
        depth_ = 1;
        size_ = 0;
        unique_keys_ = 0;
        
        // Initialize root as leaf
        Node root;
        root.init_leaf(manager_.open_block(root_id_));
        root.info->id = root_id_;
        root.info->next_id = root_id_;
        
        // Initialize LOL_FAT fast-path
        fp_id_ = root_id_;
        fp_path_[0] = fp_id_;
        fp_min_ = {};
        fp_max_ = {};
        lol_prev_id_ = INVALID_NODE_ID;
        lol_prev_min_ = {};
        lol_prev_size_ = 0;
        lol_size_ = 0;
    }
    
    void insert(const KeyT& key, const ValueT& value) {
        Node leaf;
        
        // LOL_FAT fast-path: skip tree traversal if key fits in cached leaf
        if ((fp_id_ == head_id_ || fp_min_ <= key) &&
            (fp_id_ == tail_id_ || key < fp_max_)) {
            leaf.load(manager_.open_block(fp_id_));
            // LOL_RESET: mark success
            life_.success();
            leaf_insert(leaf, fp_path_, key, value);
            return;
        }
        
        // Slow path: full tree traversal
        path_t path;
        KeyT leaf_max = find_leaf(leaf, path, key);
        
        // LOL_FAT: check if we should move the fast-path pointer (soft reset)
        if (lol_prev_id_ != INVALID_NODE_ID &&
            fp_id_ != tail_id_ &&
            fp_max_ == leaf.keys[0] &&
            dist_(fp_max_, fp_min_) < IKR::upper_bound(dist_(fp_min_, lol_prev_min_), lol_prev_size_, lol_size_)) {
            // Soft reset: move lol to lol->next
            lol_prev_min_ = fp_min_;
            lol_prev_size_ = lol_size_;
            lol_prev_id_ = fp_id_;
            fp_id_ = leaf.info->id;
            fp_min_ = fp_max_;
            fp_max_ = leaf_max;
            lol_size_ = leaf.info->size;
            fp_path_ = path;
            life_.reset();
        } else if (life_.failure()) {
            // LOL_RESET: hard reset - fast-path is ineffective, reset to current leaf
            lol_prev_id_ = INVALID_NODE_ID;
            fp_id_ = leaf.info->id;
            fp_min_ = leaf.keys[0];
            fp_max_ = leaf_max;
            lol_size_ = leaf.info->size;
            fp_path_ = path;
            life_.reset();
        }
        leaf_insert(leaf, path, key, value);
    }
    
    void range_query(const KeyT& min_key, const KeyT& max_key, vector<ValueT>& results) const {
        Node leaf;
        path_t path;
        const_cast<BPlusTreeQIT*>(this)->find_leaf(leaf, path, min_key);
        uint16_t idx = leaf.value_slot(min_key);
        
        while (true) {
            for (; idx < leaf.info->size; ++idx) {
                if (leaf.keys[idx] > max_key) 
                    return;
                uint32_t offset = leaf.offsets[idx];
                results.insert(results.end(), values_.begin() + offset, values_.begin() + offset + leaf.counts[idx]);
            }
            if (leaf.info->id == tail_id_) 
                break;
            leaf.load(const_cast<BlockManager&>(manager_).open_block(leaf.info->next_id));
            idx = 0;
        }
    }
    
    // Q1: range [min_key, max_key) - all records in range are results
    template<typename IdT>
    void q1_query_ids(const KeyT& min_key, const KeyT& max_key, vector<IdT>& results, size_t& count) const {
        if (min_key >= max_key) return;
        Node leaf;
        path_t path;
        const_cast<BPlusTreeQIT*>(this)->find_leaf(leaf, path, min_key);
        uint16_t idx = leaf.value_slot(min_key);
        
        while (true) {
            for (; idx < leaf.info->size; ++idx) {
                if (leaf.keys[idx] >= max_key) return;
                uint32_t offset = leaf.offsets[idx];
                uint32_t cnt = leaf.counts[idx];
                for (uint32_t i = 0; i < cnt; ++i) {
                    results.push_back(values_[offset + i].id);
                    count++;
                }
            }
            if (leaf.info->id == tail_id_) return;
            leaf.load(const_cast<BlockManager&>(manager_).open_block(leaf.info->next_id));
            idx = 0;
        }
    }
    
    // Q2 that writes RecordIds directly
    template<typename IdT>
    void q2_query_ids(const KeyT& min_key, const KeyT& max_key, const KeyT& filter_tb, vector<IdT>& results, size_t& count) const {
        if (min_key > max_key) return;
        Node leaf;
        path_t path;
        const_cast<BPlusTreeQIT*>(this)->find_leaf(leaf, path, min_key);
        uint16_t idx = leaf.value_slot(min_key);
        
        while (true) {
            for (; idx < leaf.info->size; ++idx) {
                if (leaf.keys[idx] > max_key) 
                    return;
                if (leaf.keys[idx] < min_key)
                    continue;
                uint32_t offset = leaf.offsets[idx];
                uint32_t cnt = leaf.counts[idx];
                KeyT te = leaf.keys[idx];
                for (uint32_t i = 0; i < cnt; ++i) {
                    const ValueT& val = values_[offset + i];
                    // Overlap: start <= qe, i.e., te - dur <= qe, i.e., te - qe <= dur
                    if (te - filter_tb <= val.duration) {
                        results.push_back(val.id);
                        count++;
                    }
                }
            }
            if (leaf.info->id == tail_id_) 
                break;
            leaf.load(const_cast<BlockManager&>(manager_).open_block(leaf.info->next_id));
            idx = 0;
        }
    }

    size_t size() const { return size_; }
    size_t unique_keys() const { return unique_keys_; }
    size_t depth() const { return depth_; }
    
    size_t getSize() const {
        size_t total = sizeof(*this);
        total += manager_.getSize();
        total += values_.size() * sizeof(ValueT);
        return total;
    }
};

#endif // BPLUS_TREE_QIT_HPP
