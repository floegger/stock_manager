#ifndef BATCH_H
#define BATCH_H

#include "hash_table.h"
#include "menu.h"

#include <string>
#include <vector>

struct BatchResult {
    struct Entry {
        std::string filename;
        std::string name;
        std::string ticker;
        std::string wkn;
        bool        ok      = false;
        std::string errMsg;
    };

    std::vector<Entry> entries;
    int imported = 0;
    int skipped  = 0;
    int failed   = 0;
};

// Parse <name>_<ticker>_<wkn>.csv out of a bare filename (no directory part).
// Returns false and leaves name/ticker/wkn untouched if the stem does not
// contain at least two underscores or does not end with ".csv".
bool parse_csv_filename ( const std::string &filename,
                          std::string       &name,
                          std::string       &ticker,
                          std::string       &wkn );

// Scan *dir* for files matching <name>_<ticker>_<wkn>.csv, create a Stock for
// each one (skipping duplicates already in *ht*), call importCSV(), and insert
// both the name key and the ticker key into *ht*.
// All newly created Stock objects are appended to *owner*.
BatchResult batch_import ( const std::string &dir,
                           HashTable         &ht,
                           owner_t           &owner );

#endif  // BATCH_H
