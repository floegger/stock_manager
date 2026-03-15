#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "stock.h"
#include "table_stats.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <stdexcept>
#include <string>

class HashTable {
  public:
    static constexpr uint8_t EMPTY = 0x80;
    static constexpr uint8_t DELETED = 0xFE;

    explicit HashTable ( size_t capacity = 64 );

    // delete disabled - requires rehash
    HashTable ( const HashTable & ) = delete;
    HashTable &operator= ( const HashTable & ) = delete;

    HashTable ( HashTable && ) noexcept = default;
    HashTable &operator= ( HashTable && ) noexcept = default;

    void ht_insert ( const std::string &key, Stock *stock );
    Stock *ht_lookup ( const std::string &key ) const;
    bool ht_delete ( const std::string &key );

    std::vector<std::string> listAll () const;

    void save ( const std::string &filename ) const;
    std::vector<std::unique_ptr<Stock>> load ( const std::string &filename );

    std::size_t size () const noexcept { return size_; }
    std::size_t capacity () const noexcept { return ctrl_.size(); }

    // Collect structural statistics and run a micro-benchmark on all live keys.
    // benchRounds: how many full insert+lookup+delete cycles to time per key.
    TableStats tableStats ( int benchRounds = 200 ) const;

  private:
    static constexpr double MAX_LOAD = 0.75;
    static constexpr std::size_t GROUP_SIZE = 32;
    static constexpr double MAX_TOMB = .20; // threshold for rehashing on delete

    std::size_t deleted_ = 0;

    struct Entry {
        std::string key;
        uint64_t hash = 0;
        Stock *stock = nullptr;
    };

    // helpers
    static uint64_t hashString ( const std::string &key ) noexcept;
    static uint8_t fingerprint ( uint64_t hash ) noexcept;
    std::ptrdiff_t probe ( const std::string &key, uint64_t hash, bool insert ) const noexcept;
    void rehash ( std::size_t newCapacity );

    std::vector<uint8_t> ctrl_;
    std::vector<Entry> entries_;
    std::size_t size_ = 0;


};  // Class HashTable

#endif
