#ifndef TABLE_STATS_H
#define TABLE_STATS_H

#include <cstddef>

struct TableStats {
    std::size_t entries     = 0;    // occupied slots
    std::size_t capacity    = 0;    // total slots
    std::size_t deleted     = 0;    // tombstone slots
    double      loadFactor  = 0.0;  // entries / capacity
    double      avgProbeLen = 0.0;  // average probe steps to reach a live key
    double      avgInsertNs = 0.0;  // nanoseconds per insert  (benchmark)
    double      avgLookupNs = 0.0;  // nanoseconds per lookup  (benchmark)
    double      avgDeleteNs = 0.0;  // nanoseconds per delete  (benchmark)
};

#endif  // TABLE_STATS_H
