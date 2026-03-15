#include "../include/batch.h"

#include "../include/stock.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// parse_csv_filename
// ---------------------------------------------------------------------------
// Expected stem format:  <name>_<ticker>_<wkn>
// The name itself may contain underscores, so we split from the right:
//   - last segment  = wkn
//   - second-to-last = ticker
//   - everything before the second-to-last underscore = name
// Underscores in the name are converted back to spaces.
// ---------------------------------------------------------------------------
bool parse_csv_filename ( const std::string &filename,
                          std::string       &name,
                          std::string       &ticker,
                          std::string       &wkn ) {
    // Must end with ".csv" (case-sensitive, matching the download script)
    if ( filename.size() < 5 )
        return false;
    if ( filename.compare ( filename.size() - 4, 4, ".csv" ) != 0 )
        return false;

    // Strip the extension to get the stem
    std::string stem = filename.substr ( 0, filename.size() - 4 );

    // Find the last underscore  →  wkn
    auto pos2 = stem.rfind ( '_' );
    if ( pos2 == std::string::npos || pos2 == 0 )
        return false;

    // Find the second-to-last underscore  →  ticker
    auto pos1 = stem.rfind ( '_', pos2 - 1 );
    if ( pos1 == std::string::npos || pos1 == 0 )
        return false;

    // Extract the three parts
    std::string rawName   = stem.substr ( 0, pos1 );
    std::string rawTicker = stem.substr ( pos1 + 1, pos2 - pos1 - 1 );
    std::string rawWkn    = stem.substr ( pos2 + 1 );

    if ( rawName.empty() || rawTicker.empty() || rawWkn.empty() )
        return false;

    // Replace underscores with spaces in the name (the download script
    // sanitised spaces → underscores)
    std::replace ( rawName.begin(), rawName.end(), '_', ' ' );

    name   = std::move ( rawName );
    ticker = std::move ( rawTicker );
    wkn    = std::move ( rawWkn );
    return true;
}

// ---------------------------------------------------------------------------
// batch_import
// ---------------------------------------------------------------------------
BatchResult batch_import ( const std::string &dir,
                           HashTable         &ht,
                           owner_t           &owner ) {
    BatchResult result;

    fs::path dirPath ( dir );
    if ( !fs::is_directory ( dirPath ) ) {
        // Return an empty result with a single failed pseudo-entry so the
        // caller can display a useful message.
        BatchResult::Entry e;
        e.filename = dir;
        e.ok       = false;
        e.errMsg   = "Not a directory";
        result.entries.push_back ( std::move ( e ) );
        ++result.failed;
        return result;
    }

    for ( const auto &dirEntry : fs::directory_iterator ( dirPath ) ) {
        if ( !dirEntry.is_regular_file() )
            continue;

        BatchResult::Entry entry;
        entry.filename = dirEntry.path().filename().string();

        // ── parse filename ────────────────────────────────────────────────
        if ( !parse_csv_filename ( entry.filename, entry.name, entry.ticker, entry.wkn ) ) {
            entry.ok     = false;
            entry.errMsg = "Filename does not match <name>_<ticker>_<wkn>.csv";
            result.entries.push_back ( std::move ( entry ) );
            ++result.skipped;
            continue;
        }

        // ── skip duplicates ───────────────────────────────────────────────
        // A stock is considered already loaded if either the ticker or the
        // name key is already in the table.
        if ( ht.ht_lookup ( entry.ticker ) || ht.ht_lookup ( entry.name ) ) {
            entry.ok     = false;
            entry.errMsg = "Already in table – skipped";
            result.entries.push_back ( std::move ( entry ) );
            ++result.skipped;
            continue;
        }

        // ── create Stock + import CSV ─────────────────────────────────────
        try {
            auto stock = std::make_unique<Stock> ( entry.name, entry.wkn, entry.ticker );
            stock->importCSV ( dirEntry.path().string() );

            // Insert under both name and ticker so both keys resolve to it
            ht.ht_insert ( entry.name,   stock.get() );
            ht.ht_insert ( entry.ticker, stock.get() );

            owner.push_back ( std::move ( stock ) );

            entry.ok = true;
            ++result.imported;
        } catch ( const std::exception &ex ) {
            entry.ok     = false;
            entry.errMsg = ex.what();
            ++result.failed;
        }

        result.entries.push_back ( std::move ( entry ) );
    }

    return result;
}
