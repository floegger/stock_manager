#include "../include/menu.h"
#include "../include/hash_table.h"
#include "../include/plot.h"
#include "../include/stock.h"
#include "../include/tui.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

void run_menu ( HashTable &ht, owner_t &owner ) {
    std::vector<std::string> output;
    std::string status;

    while ( true ) {
        std::string line = draw_screen ( output, status );
        if ( line.empty() && std::cin.eof() )
            break;

        std::istringstream ss ( line );
        std::string cmd;
        ss >> cmd;

        output.clear();
        status.clear();

        if ( cmd == "9" || cmd == "QUIT" )
            break;

        //  ADD
        if ( cmd == "1" || cmd == "ADD" ) {
            auto name = input_screen ( output, "ADD – enter stock details", "Stock name:" );
            auto wkn = input_screen ( output, "ADD – " + name, "WKN:" );
            auto symbol = input_screen ( output, "ADD – " + name, "Symbol:" );
            try {
                auto s = std::make_unique<Stock> ( name, wkn, symbol );
                ht.ht_insert ( name, s.get() );
                ht.ht_insert ( symbol, s.get() );
                owner.push_back ( std::move ( s ) );
                status = "Added: " + name + "  (" + symbol + ")";
            } catch ( const std::exception &e ) {
                status = std::string ( "Error: " ) + e.what();
            }

            //  DEL
        } else if ( cmd == "2" || cmd == "DEL" ) {
            auto key = input_screen ( output, status, "Name or symbol to delete:" );
            if ( Stock *st = ht.ht_lookup ( key ) ) {
                std::string name = st->getName();
                std::string symbol = st->getSymbol();
                ht.ht_delete ( name );
                ht.ht_delete ( symbol );
                owner.erase (
                    std::remove_if ( owner.begin(), owner.end(), [ & ] ( const auto &p ) { return p.get() == st; } ),
                    owner.end() );
                status = "Deleted: " + name + "  (" + symbol + ")";
            } else {
                status = "Not found: " + key;
            }

            //  IMPORT
        } else if ( cmd == "3" || cmd == "IMPORT" ) {
            auto key = input_screen ( output, status, "Name or symbol:" );
            auto file = input_screen ( output, "IMPORT – " + key, "CSV file path:" );
            if ( Stock *st = ht.ht_lookup ( key ) ) {
                try {
                    st->importCSV ( file );
                    status = "Imported CSV for: " + st->getName();
                } catch ( const std::exception &e ) {
                    status = std::string ( "Error: " ) + e.what();
                }
            } else {
                status = "Stock not found: " + key;
            }

            //  SEARCH
        } else if ( cmd == "4" || cmd == "SEARCH" ) {
            auto key = input_screen ( output, status, "Name or symbol:" );
            if ( const Stock *st = ht.ht_lookup ( key ) ) {
                output.push_back ( "  Name:   " + st->getName() );
                output.push_back ( "  Symbol: " + st->getSymbol() );
                output.push_back ( "  WKN:    " + st->getWKN() );
                if ( st->hasHistory() ) {
                    const auto &e = st->latest();
                    output.push_back ( "" );
                    output.push_back ( "  Latest  (" + e.date + ")" );
                    output.push_back ( "    Open:   " + std::to_string ( e.open ) );
                    output.push_back ( "    High:   " + std::to_string ( e.high ) );
                    output.push_back ( "    Low:    " + std::to_string ( e.low ) );
                    output.push_back ( "    Close:  " + std::to_string ( e.close ) );
                    output.push_back ( "    Volume: " + std::to_string ( e.volume ) );
                } else {
                    output.push_back ( "" );
                    output.push_back ( "  No price history available." );
                }
                status = "Search result for: " + key;
            } else {
                status = "Not found: " + key;
            }

            //  PLOT
        } else if ( cmd == "5" || cmd == "PLOT" ) {
            auto key = input_screen ( output, status, "Name or symbol:" );
            if ( const Stock *st = ht.ht_lookup ( key ) ) {
                output = plotStock ( *st );
                status = "Plot: " + st->getName();
            } else {
                status = "Not found: " + key;
            }

            //  LIST
        } else if ( cmd == "6" || cmd == "LIST" ) {
            output = ht.listAll();
            status = std::to_string ( ht.size() ) + " stock(s) in table";

            //  SAVE
        } else if ( cmd == "7" || cmd == "SAVE" ) {
            auto file = input_screen ( output, status, "Save to file:" );
            try {
                ht.save ( file );
                status = "Saved " + std::to_string ( ht.size() ) + " stock(s) to " + file;
            } catch ( const std::exception &e ) {
                status = std::string ( "Error: " ) + e.what();
            }

            //  LOAD
        } else if ( cmd == "8" || cmd == "LOAD" ) {
            auto file = input_screen ( output, status, "Load from file:" );
            try {
                auto stocks = ht.load ( file );
                std::size_t n = stocks.size();
                for ( auto &s : stocks ) {
                    ht.ht_insert ( s->getName(), s.get() );
                    ht.ht_insert ( s->getSymbol(), s.get() );
                    owner.push_back ( std::move ( s ) );
                }
                status = "Loaded " + std::to_string ( n ) + " stock(s) from " + file;
            } catch ( const std::exception &e ) {
                status = std::string ( "Error: " ) + e.what();
            }

            //  unknown
        } else if ( !cmd.empty() ) {
            status = "Unknown command: " + cmd;
        }
    }
}
