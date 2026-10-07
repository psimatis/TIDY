#ifndef _RTREE_HPP_
#define _RTREE_HPP_

#include "def_global.h"
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <boost/geometry/index/detail/rtree/utilities/statistics.hpp>

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

typedef bg::model::point<Timestamp, 2, bg::cs::cartesian> RTreePoint;
typedef bg::model::box<RTreePoint> RTreeBox;
typedef pair<RTreePoint, RecordId> RTreeValue;
typedef bgi::rtree<RTreeValue, bgi::rstar<CAPACITY>> RTreeIndex;

class RTree {
public:
    RTreeIndex rtree;

    void insert(Timestamp start, Timestamp end) {
        RTreePoint point(start, end);
        rtree.insert(make_pair(point, rtree.size()));
    }

    void query(Timestamp query_start, Timestamp query_end, vector<RTreeValue>& results) {
        RTreeBox queryBox(RTreePoint(0, query_start), RTreePoint(query_end, INFINITY));
        rtree.query(bgi::intersects(queryBox), back_inserter(results));
    }

    size_t query_count(Timestamp query_start, Timestamp query_end) {
        vector<RTreeValue> results;
        query(query_start, query_end, results);
        return results.size();
    }

    float memory_usage_mb() const {
        auto stats = bg::index::detail::rtree::utilities::statistics(rtree);
        return (((get<1>(stats) + get<2>(stats)) * 4 * (sizeof(int) + sizeof(int))) + get<3>(stats) * sizeof(RTreeValue)) / float(1e6);
    }
};

#endif // _RTREE_HPP_
