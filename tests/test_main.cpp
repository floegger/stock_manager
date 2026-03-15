#include "../include/batch.h"
#include "../include/hash_table.h"
#include "../include/stock.h"

#include <cassert>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ── Minimal test framework ────────────────────────────────────────

static int s_passed = 0;
static int s_failed = 0;

#define CHECK(expr)                                                                      \
    do {                                                                                 \
        if ( expr ) {                                                                    \
            ++s_passed;                                                                  \
        } else {                                                                         \
            ++s_failed;                                                                  \
            std::cerr << "  FAIL  " << __FILE__ << ":" << __LINE__ << "  " << #expr << '\n'; \
        }                                                                                \
    } while ( 0 )

#define CHECK_THROWS(expr)                                                               \
    do {                                                                                 \
        bool threw = false;                                                              \
        try { (void)(expr); } catch ( ... ) { threw = true; }                           \
        if ( threw ) {                                                                   \
            ++s_passed;                                                                  \
        } else {                                                                         \
            ++s_failed;                                                                  \
            std::cerr << "  FAIL  " << __FILE__ << ":" << __LINE__                      \
                      << "  expected exception: " << #expr << '\n';                     \
        }                                                                                \
    } while ( 0 )

#define RUN(fn)                                                                          \
    do {                                                                                 \
        std::cout << "[ RUN  ] " << #fn << '\n';                                        \
        fn();                                                                            \
        std::cout << "[  OK  ] " << #fn << '\n';                                        \
    } while ( 0 )

// ── Batch / filename parser tests ────────────────────────────────

// Happy path: plain name with no extra underscores
void test_parse_csv_filename_normal () {
    std::string name, ticker, wkn;
    CHECK ( parse_csv_filename ( "Apple_Inc._Common_Stock_AAPL_037833.csv", name, ticker, wkn ) );
    CHECK ( name   == "Apple Inc. Common Stock" );
    CHECK ( ticker == "AAPL" );
    CHECK ( wkn    == "037833" );
}

// Name that itself contains underscores (the download script replaces spaces
// with underscores, so multiple underscores in the name portion are expected)
void test_parse_csv_filename_underscores_in_name () {
    std::string name, ticker, wkn;
    CHECK ( parse_csv_filename ( "NVIDIA_Corporation_Common_Stock_NVDA_NVDA.csv", name, ticker, wkn ) );
    CHECK ( name   == "NVIDIA Corporation Common Stock" );
    CHECK ( ticker == "NVDA" );
    CHECK ( wkn    == "NVDA" );
}

// Missing ".csv" extension must be rejected
void test_parse_csv_filename_no_extension () {
    std::string name, ticker, wkn;
    CHECK ( !parse_csv_filename ( "Apple_AAPL_037833", name, ticker, wkn ) );
}

// Wrong extension must be rejected
void test_parse_csv_filename_wrong_extension () {
    std::string name, ticker, wkn;
    CHECK ( !parse_csv_filename ( "Apple_AAPL_037833.txt", name, ticker, wkn ) );
}

// Stem with only one underscore (cannot split into three parts)
void test_parse_csv_filename_too_few_underscores () {
    std::string name, ticker, wkn;
    CHECK ( !parse_csv_filename ( "AAPL_037833.csv", name, ticker, wkn ) );
}

// Any segment being empty after splitting must be rejected
void test_parse_csv_filename_empty_segment () {
    std::string name, ticker, wkn;
    // leading underscore → empty name
    CHECK ( !parse_csv_filename ( "_AAPL_037833.csv", name, ticker, wkn ) );
    // empty ticker
    CHECK ( !parse_csv_filename ( "Apple__037833.csv", name, ticker, wkn ) );
    // empty wkn
    CHECK ( !parse_csv_filename ( "Apple_AAPL_.csv", name, ticker, wkn ) );
}

// Minimal valid filename: one-word name, ticker, wkn — no extra underscores
void test_parse_csv_filename_minimal () {
    std::string name, ticker, wkn;
    CHECK ( parse_csv_filename ( "Foo_BAR_123456.csv", name, ticker, wkn ) );
    CHECK ( name   == "Foo" );
    CHECK ( ticker == "BAR" );
    CHECK ( wkn    == "123456" );
}

// ── Stock tests ───────────────────────────────────────────────────

void test_stock_construction () {
    Stock s ( "Apple Inc.", "865985", "AAPL" );
    CHECK ( s.getName()   == "Apple Inc." );
    CHECK ( s.getWKN()    == "865985" );
    CHECK ( s.getSymbol() == "AAPL" );
    CHECK ( !s.hasHistory() );
}

void test_stock_empty_fields_throw () {
    CHECK_THROWS ( Stock ( "", "865985", "AAPL" ) );
    CHECK_THROWS ( Stock ( "Apple Inc.", "", "AAPL" ) );
    CHECK_THROWS ( Stock ( "Apple Inc.", "865985", "" ) );
}

void test_stock_latest_throws_when_empty () {
    Stock s ( "Apple Inc.", "865985", "AAPL" );
    CHECK_THROWS ( s.latest() );
}

void test_stock_save_load_roundtrip () {
    Stock original ( "Apple Inc.", "865985", "AAPL" );

    // Build two price entries manually via save/load round-trip
    std::ostringstream oss;
    // Write a minimal hand-crafted stream that loadFromFile can read
    oss << "Apple Inc.\n" << "865985\n" << "AAPL\n";
    oss << "2\n";
    oss << "2024-01-01,185.50,1000000,183.00,186.00,182.00\n";
    oss << "2024-01-02,187.20,1200000,185.50,188.00,185.00\n";

    std::istringstream iss ( oss.str() );
    Stock loaded = Stock::loadFromFile ( iss );

    CHECK ( loaded.getName()   == "Apple Inc." );
    CHECK ( loaded.getWKN()    == "865985" );
    CHECK ( loaded.getSymbol() == "AAPL" );
    CHECK ( loaded.hasHistory() );
    CHECK ( loaded.history().size() == 2 );
    CHECK ( loaded.latest().date   == "2024-01-02" );
    CHECK ( loaded.latest().close  == 187.20 );
    CHECK ( loaded.latest().volume == 1200000 );
    CHECK ( loaded.latest().open   == 185.50 );
    CHECK ( loaded.latest().high   == 188.00 );
    CHECK ( loaded.latest().low    == 185.00 );
}

void test_stock_save_produces_loadable_output () {
    // Round-trip: saveToFile -> loadFromFile
    std::ostringstream oss;
    oss << "Tesla Inc.\n" << "A1CX3T\n" << "TSLA\n";
    oss << "1\n";
    oss << "2024-03-01,200.00,500000,198.00,202.00,197.00\n";

    std::istringstream iss ( oss.str() );
    Stock s = Stock::loadFromFile ( iss );

    std::ostringstream saved;
    s.saveToFile ( saved );

    std::istringstream reloaded ( saved.str() );
    Stock s2 = Stock::loadFromFile ( reloaded );

    CHECK ( s2.getName()   == s.getName() );
    CHECK ( s2.getWKN()    == s.getWKN() );
    CHECK ( s2.getSymbol() == s.getSymbol() );
    CHECK ( s2.history().size() == s.history().size() );
    CHECK ( s2.latest().close   == s.latest().close );
}

// ── HashTable tests ───────────────────────────────────────────────

void test_ht_insert_and_lookup () {
    HashTable ht;
    Stock s ( "Apple Inc.", "865985", "AAPL" );

    ht.ht_insert ( "AAPL", &s );
    CHECK ( ht.ht_lookup ( "AAPL" ) == &s );
    CHECK ( ht.size() == 1 );
}

void test_ht_lookup_missing_returns_nullptr () {
    HashTable ht;
    CHECK ( ht.ht_lookup ( "MISSING" ) == nullptr );
}

void test_ht_insert_duplicate_updates () {
    HashTable ht;
    Stock s1 ( "Apple Inc.", "865985", "AAPL" );
    Stock s2 ( "Apple Ltd.", "865986", "APLX" );

    ht.ht_insert ( "AAPL", &s1 );
    ht.ht_insert ( "AAPL", &s2 );  // same key, different stock

    CHECK ( ht.size() == 1 );                        // no new entry
    CHECK ( ht.ht_lookup ( "AAPL" ) == &s2 );        // updated pointer
}

void test_ht_delete () {
    HashTable ht;
    Stock s ( "Apple Inc.", "865985", "AAPL" );

    ht.ht_insert ( "AAPL", &s );
    CHECK ( ht.ht_delete ( "AAPL" ) == true );
    CHECK ( ht.ht_lookup ( "AAPL" ) == nullptr );
    CHECK ( ht.size() == 0 );
}

void test_ht_delete_missing_returns_false () {
    HashTable ht;
    CHECK ( ht.ht_delete ( "NOBODY" ) == false );
}

void test_ht_multi_key_same_stock () {
    HashTable ht;
    Stock s ( "Apple Inc.", "865985", "AAPL" );

    ht.ht_insert ( s.getName(),   &s );
    ht.ht_insert ( s.getSymbol(), &s );

    CHECK ( ht.size() == 2 );
    CHECK ( ht.ht_lookup ( "Apple Inc." ) == &s );
    CHECK ( ht.ht_lookup ( "AAPL" )       == &s );
}

void test_ht_many_inserts_trigger_rehash () {
    // Insert enough entries to cross the 0.75 load threshold several times
    HashTable ht ( 8 );
    std::vector<Stock> stocks;
    stocks.reserve ( 100 );

    for ( int i = 0; i < 100; ++i ) {
        std::string name = "Stock" + std::to_string ( i );
        stocks.emplace_back ( name, "WKN" + std::to_string ( i ), "S" + std::to_string ( i ) );
    }
    for ( auto &s : stocks )
        ht.ht_insert ( s.getName(), &s );

    CHECK ( ht.size() == 100 );
    for ( auto &s : stocks )
        CHECK ( ht.ht_lookup ( s.getName() ) == &s );
}

void test_ht_insert_after_delete_reuses_slot () {
    HashTable ht ( 8 );
    Stock s1 ( "Alpha Corp", "AAA001", "ALPH" );
    Stock s2 ( "Beta Corp",  "BBB002", "BETA" );

    ht.ht_insert ( "ALPH", &s1 );
    ht.ht_delete ( "ALPH" );
    ht.ht_insert ( "BETA", &s2 );

    CHECK ( ht.ht_lookup ( "ALPH" ) == nullptr );
    CHECK ( ht.ht_lookup ( "BETA" ) == &s2 );
    CHECK ( ht.size() == 1 );
}

void test_ht_zero_capacity_throws () {
    CHECK_THROWS ( HashTable ( 0 ) );
}

void test_ht_save_load_roundtrip () {
    // Build a table with two stocks, save, reload, verify
    std::vector<std::unique_ptr<Stock>> owner;
    HashTable ht;

    owner.push_back ( std::make_unique<Stock> ( "Apple Inc.", "865985", "AAPL" ) );
    owner.push_back ( std::make_unique<Stock> ( "Tesla Inc.", "A1CX3T", "TSLA" ) );
    for ( auto &s : owner ) {
        ht.ht_insert ( s->getName(),   s.get() );
        ht.ht_insert ( s->getSymbol(), s.get() );
    }

    const std::string tmpfile = "/tmp/ht_test_save.dat";
    ht.save ( tmpfile );

    HashTable ht2;
    auto loaded = ht2.load ( tmpfile );
    for ( auto &s : loaded ) {
        ht2.ht_insert ( s->getName(),   s.get() );
        ht2.ht_insert ( s->getSymbol(), s.get() );
    }

    // save() serialises every occupied slot, so name+symbol for 2 stocks = 4 entries
    CHECK ( loaded.size() == 4 );
    CHECK ( ht2.ht_lookup ( "Apple Inc." ) != nullptr );
    CHECK ( ht2.ht_lookup ( "AAPL" )       != nullptr );
    CHECK ( ht2.ht_lookup ( "Tesla Inc." ) != nullptr );
    CHECK ( ht2.ht_lookup ( "TSLA" )       != nullptr );
    CHECK ( ht2.ht_lookup ( "Apple Inc." )->getSymbol() == "AAPL" );
    CHECK ( ht2.ht_lookup ( "TSLA" )->getName()         == "Tesla Inc." );
}

// ── Entry point ───────────────────────────────────────────────────

int main () {
    std::cout << "\n=== Batch / filename parser tests ===\n";
    RUN ( test_parse_csv_filename_normal );
    RUN ( test_parse_csv_filename_underscores_in_name );
    RUN ( test_parse_csv_filename_no_extension );
    RUN ( test_parse_csv_filename_wrong_extension );
    RUN ( test_parse_csv_filename_too_few_underscores );
    RUN ( test_parse_csv_filename_empty_segment );
    RUN ( test_parse_csv_filename_minimal );

    std::cout << "\n=== Stock tests ===\n";
    RUN ( test_stock_construction );
    RUN ( test_stock_empty_fields_throw );
    RUN ( test_stock_latest_throws_when_empty );
    RUN ( test_stock_save_load_roundtrip );
    RUN ( test_stock_save_produces_loadable_output );

    std::cout << "\n=== HashTable tests ===\n";
    RUN ( test_ht_insert_and_lookup );
    RUN ( test_ht_lookup_missing_returns_nullptr );
    RUN ( test_ht_insert_duplicate_updates );
    RUN ( test_ht_delete );
    RUN ( test_ht_delete_missing_returns_false );
    RUN ( test_ht_multi_key_same_stock );
    RUN ( test_ht_many_inserts_trigger_rehash );
    RUN ( test_ht_insert_after_delete_reuses_slot );
    RUN ( test_ht_zero_capacity_throws );
    RUN ( test_ht_save_load_roundtrip );

    std::cout << "\n────────────────────────────────\n";
    std::cout << "  Passed: " << s_passed << "\n";
    std::cout << "  Failed: " << s_failed << "\n";
    std::cout << "────────────────────────────────\n\n";

    return s_failed == 0 ? 0 : 1;
}
