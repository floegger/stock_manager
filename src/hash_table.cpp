#include "../include/hash_table.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iostream>

#ifdef __AVX2__
#include <immintrin.h>
#endif

HashTable::HashTable ( std::size_t capacity ) : ctrl_ ( capacity, EMPTY ), entries_ ( capacity ), size_ ( 0 ) {
    if ( capacity == 0 ) {
        throw std::invalid_argument ( "Capacity must be > 0" );
    }
}  // HashTable

/* === Hashing and fingerprinting === */

uint64_t HashTable::hashString ( const std::string &key ) noexcept {
    uint64_t hash = 14695981039346656037ULL;  // FNV-1a offset basis
    for ( char c : key ) {
        hash ^= static_cast<uint8_t> ( c );
        hash *= 1099511628211ULL;  // FNV-1a prime
    }
    return hash;
}

uint8_t HashTable::fingerprint ( uint64_t hash ) noexcept {
    return static_cast<uint8_t> ( hash & 0x7F );  // Use lower 7 bits for fingerprint
}

/* === Probe function === */

// insert = true -> insert, insert = false -> lookup
std::ptrdiff_t HashTable::probe ( const std::string &key, uint64_t hash, bool insert ) const noexcept {
    const std::size_t capacity = ctrl_.size();
    const uint8_t fp = fingerprint ( hash );

#ifdef __AVX2__
    std::size_t group = ( hash % capacity ) & ~( GROUP_SIZE - 1 );  // Align to group boundary
    __m256i fpVec = _mm256_set1_epi8 ( fp );
    __m256i emptyVec = _mm256_set1_epi8 ( static_cast<char> ( EMPTY ) );

    std::ptrdiff_t firstTombstone = -1;

    for ( ;; ) {
        __m256i ctrlVec = _mm256_loadu_si256 ( reinterpret_cast<const __m256i *> ( &ctrl_[ group ] ) );
        uint32_t matchMask = _mm256_movemask_epi8 ( _mm256_cmpeq_epi8 ( ctrlVec, fpVec ) );
        uint32_t emptyMask = _mm256_movemask_epi8 ( _mm256_cmpeq_epi8 ( ctrlVec, emptyVec ) );

        // check candidates
        for ( uint32_t mask = matchMask; mask != 0; mask &= ( mask - 1 ) ) {
            std::ptrdiff_t idx = ( group + static_cast<std::size_t> ( __builtin_ctz ( mask ) ) ) % capacity;

            if ( entries_[ idx ].hash == hash && entries_[ idx ].key == key ) {
                return static_cast<std::ptrdiff_t> ( idx );  // Found
            }
        }
        // record first tombstone for this group
        if ( insert && firstTombstone == -1 ) {
            for ( std::size_t i = 0; i < GROUP_SIZE; ++i ) {
                std::size_t idx = ( group + i ) % capacity;
                if ( ctrl_[ idx ] == DELETED ) {
                    firstTombstone = static_cast<std::ptrdiff_t> ( idx );
                    break;
                }
            }
        }

        if ( emptyMask ) {
            if ( !insert )
                return -1;  // Not found
            std::size_t slot = ( group + static_cast<std::size_t> ( __builtin_ctz ( emptyMask ) ) ) % capacity;
            return firstTombstone != -1 ? firstTombstone : static_cast<std::ptrdiff_t> ( slot );  // Insert here
        }
        group = ( group + GROUP_SIZE ) % capacity;  // Move to next group
    }
#else  // Fallback to scalar probing
    std::size_t group = ( hash % capacity ) & ~( GROUP_SIZE - 1 );  // Align to group boundary
    std::ptrdiff_t firstTombstone = -1;

    for ( ;; ) {
        bool foundEmpty = false;
        std::size_t emptyIdx = 0;

        for ( std::size_t i = 0; i < GROUP_SIZE; ++i ) {
            std::size_t idx = ( group + i ) % capacity;
            uint8_t c = ctrl_[ idx ];

            if ( c == EMPTY ) {
                if ( !foundEmpty ) {
                    foundEmpty = true;
                    emptyIdx = idx;
                }
            } else if ( c == DELETED ) {
                if ( insert && firstTombstone == -1 ) {
                    firstTombstone = static_cast<std::ptrdiff_t> ( idx );
                }
            } else if ( c == fp && entries_[ idx ].hash == hash && entries_[ idx ].key == key ) {
                return static_cast<std::ptrdiff_t> ( idx );  // Found
            }
        }

        if ( foundEmpty ) {
            if ( !insert )
                return -1;  // Not found
            return firstTombstone != -1 ? firstTombstone : static_cast<std::ptrdiff_t> ( emptyIdx );  // Insert here
        }
        group = ( group + GROUP_SIZE ) % capacity;  // Move to next group
    }  // end probeloop
#endif
}  // probe

/* === Insert, lookup, delete === */

void HashTable::ht_insert ( const std::string &key, Stock *stock ) {
    if ( static_cast<double> ( size_ ) / static_cast<double> ( ctrl_.size() ) > MAX_LOAD )
        rehash ( ctrl_.size() * 2 );
    uint64_t hash = hashString ( key );
    auto idx = static_cast<std::size_t> ( probe ( key, hash, true ) );
    if ( ctrl_[ idx ] == EMPTY || ctrl_[ idx ] == DELETED ) {
        Entry tmp;
        tmp.key = key;
        tmp.hash = hash;
        tmp.stock = stock;
        entries_[ idx ] = std::move ( tmp );
        ++size_;
    } else {
        entries_[ idx ].stock = stock;  // Update existing
    }
    ctrl_[ idx ] = fingerprint ( hash );
}  // ht_insert

Stock *HashTable::ht_lookup ( const std::string &key ) const {
    uint64_t hash = hashString ( key );
    auto idx = probe ( key, hash, false );
    if ( idx != -1 ) {
        return entries_[ static_cast<std::size_t> ( idx ) ].stock;
    }
    return nullptr;  // Not found
}  // ht_lookup

bool HashTable::ht_delete ( const std::string &key ) {
    uint64_t hash = hashString ( key );
    auto raw = probe ( key, hash, false );

    if ( raw == -1 )
        return false;  // Not found

    auto idx = static_cast<std::size_t> ( raw );
    ctrl_[ idx ] = DELETED;
    entries_[ idx ] = Entry();  // Clear entry
    --size_;
    return true;
}  // ht_delete

/* === Rehashing === */

void HashTable::rehash ( std::size_t newCapacity ) {
    HashTable newTable ( newCapacity );
    for ( std::size_t i = 0; i < ctrl_.size(); ++i ) {
        if ( ctrl_[ i ] != EMPTY && ctrl_[ i ] != DELETED ) {
            const Entry &entry = entries_[ i ];
            newTable.ht_insert ( entry.key, entry.stock );
        }
    }
    ctrl_     = std::move ( newTable.ctrl_ );
    entries_  = std::move ( newTable.entries_ );
    size_     = newTable.size_;
}  // rehash

// === List all entries ===
std::vector<std::string> HashTable::listAll () const {
    std::vector<std::string> lines;
    for ( std::size_t i = 0; i < ctrl_.size(); ++i ) {
        if ( ctrl_[ i ] != EMPTY && ctrl_[ i ] != DELETED ) {
            const Stock *stock = entries_[ i ].stock;
            std::string line = "  " + stock->getName() +
                               "  (" + stock->getSymbol() + ", " + stock->getWKN() + ")";
            if ( stock->hasHistory() ) {
                line += "  close: " + std::to_string ( stock->latest().close ) +
                        "  on " + stock->latest().date;
            }
            lines.push_back ( line );
        }
    }
    if ( lines.empty() )
        lines.push_back ( "  (no stocks)" );
    return lines;
}  // listAll

/* === Save and load === */

void HashTable::save ( const std::string &filename ) const {
    std::ofstream file ( filename, std::ios::binary );
    if ( !file )
        throw std::runtime_error ( "Cannot write: " + filename );

    file << size_ << "\n";  // Save number of entries

    for ( std::size_t i = 0; i < ctrl_.size(); ++i ) {
        if ( ctrl_[ i ] != EMPTY && ctrl_[ i ] != DELETED ) {
            entries_[ i ].stock->saveToFile ( file );
        }
    }

}  // save

/* === Table statistics + micro-benchmark === */

TableStats HashTable::tableStats ( int benchRounds ) const {
    TableStats stats;
    stats.entries  = size_;
    stats.capacity = ctrl_.size();

    // ── structural pass ────────────────────────────────────────────────────
    std::size_t deletedCount  = 0;
    double      totalProbeLen = 0.0;

    for ( std::size_t i = 0; i < ctrl_.size(); ++i ) {
        if ( ctrl_[ i ] == DELETED ) {
            ++deletedCount;
            continue;
        }
        if ( ctrl_[ i ] == EMPTY )
            continue;

        // Walk probe sequence from the ideal slot to this slot to measure
        // how many steps were needed to place / find this key.
        const uint64_t     hash = entries_[ i ].hash;
        const std::size_t  cap  = ctrl_.size();
        std::size_t        ideal = ( hash % cap ) & ~( GROUP_SIZE - 1 );
        std::size_t        steps = 1;

        // Advance groups until we reach the group that contains slot i.
        std::size_t group = ideal;
        while ( true ) {
            // Is slot i inside [group, group+GROUP_SIZE)?
            bool inGroup = false;
            for ( std::size_t g = 0; g < GROUP_SIZE; ++g ) {
                if ( ( group + g ) % cap == i ) { inGroup = true; break; }
            }
            if ( inGroup ) break;
            group = ( group + GROUP_SIZE ) % cap;
            ++steps;
        }
        totalProbeLen += static_cast<double> ( steps );
    }

    stats.deleted     = deletedCount;
    stats.loadFactor  = stats.capacity > 0
                            ? static_cast<double> ( stats.entries ) /
                              static_cast<double> ( stats.capacity )
                            : 0.0;
    stats.avgProbeLen = stats.entries > 0
                            ? totalProbeLen / static_cast<double> ( stats.entries )
                            : 0.0;

    // ── micro-benchmark ────────────────────────────────────────────────────
    // Collect all live keys once so the benchmark loop is deterministic.
    std::vector<std::string> keys;
    keys.reserve ( size_ );
    for ( std::size_t i = 0; i < ctrl_.size(); ++i ) {
        if ( ctrl_[ i ] != EMPTY && ctrl_[ i ] != DELETED )
            keys.push_back ( entries_[ i ].key );
    }

    if ( keys.empty() || benchRounds <= 0 ) {
        return stats;
    }

    // We need a mutable copy of the table to time inserts / deletes.
    // Clone via rehash into a fresh table.
    HashTable bench ( ctrl_.size() );
    for ( std::size_t i = 0; i < ctrl_.size(); ++i ) {
        if ( ctrl_[ i ] != EMPTY && ctrl_[ i ] != DELETED )
            bench.ht_insert ( entries_[ i ].key, entries_[ i ].stock );
    }

    const long long totalOps = static_cast<long long> ( keys.size() ) * benchRounds;

    // ── lookup timing ──────────────────────────────────────────────────────
    {
        auto t0 = std::chrono::high_resolution_clock::now();
        for ( int r = 0; r < benchRounds; ++r )
            for ( const auto &k : keys )
                (void) bench.ht_lookup ( k );
        auto t1 = std::chrono::high_resolution_clock::now();
        double ns = static_cast<double> (
            std::chrono::duration_cast<std::chrono::nanoseconds> ( t1 - t0 ).count() );
        stats.avgLookupNs = ns / static_cast<double> ( totalOps );
    }

    // ── insert timing (re-inserting existing keys = update path) ──────────
    {
        auto t0 = std::chrono::high_resolution_clock::now();
        for ( int r = 0; r < benchRounds; ++r )
            for ( std::size_t ki = 0; ki < keys.size(); ++ki )
                bench.ht_insert ( keys[ ki ], entries_[
                    // find original slot index for this key
                    static_cast<std::size_t> (
                        bench.probe ( keys[ ki ], bench.hashString ( keys[ ki ] ), false )
                    ) ].stock );
        auto t1 = std::chrono::high_resolution_clock::now();
        double ns = static_cast<double> (
            std::chrono::duration_cast<std::chrono::nanoseconds> ( t1 - t0 ).count() );
        stats.avgInsertNs = ns / static_cast<double> ( totalOps );
    }

    // ── delete timing ──────────────────────────────────────────────────────
    // We delete then re-insert in each round to keep the table intact.
    {
        long long deleteOps = 0;
        auto t0 = std::chrono::high_resolution_clock::now();
        for ( int r = 0; r < benchRounds; ++r ) {
            for ( const auto &k : keys ) {
                Stock *ptr = bench.ht_lookup ( k );
                bench.ht_delete ( k );
                bench.ht_insert ( k, ptr );   // restore immediately
                ++deleteOps;
            }
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double ns = static_cast<double> (
            std::chrono::duration_cast<std::chrono::nanoseconds> ( t1 - t0 ).count() );
        // divide by 2: each iteration is one delete + one re-insert; charge only the delete
        stats.avgDeleteNs = ns / ( 2.0 * static_cast<double> ( deleteOps ) );
    }

    return stats;
}  // tableStats

std::vector<std::unique_ptr<Stock>> HashTable::load ( const std::string &filename ) {
    std::ifstream file ( filename );
    if ( !file )
        throw std::runtime_error ( "Cannot read: " + filename );
    std::fill ( ctrl_.begin(), ctrl_.end(), EMPTY );  // Clear existing data
    for ( auto &entry : entries_ ) {
        entry = Entry {};  // Clear entries
    }
    size_ = 0;
    std::size_t n = 0;
    file >> n;  // Read number of entries
    file.ignore();

    std::vector<std::unique_ptr<Stock>> result;
    result.reserve ( n );
    for ( std::size_t i = 0; i < n; ++i ) {
        result.push_back ( std::make_unique<Stock> ( Stock::loadFromFile ( file ) ) );
    }

    return result;
}
