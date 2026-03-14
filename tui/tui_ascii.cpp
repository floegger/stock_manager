// tui_ascii.cpp  –  ASCII-frame terminal UI
// Requires only a C++17-capable terminal; no external libraries.
#include "HashTable.h"
#include "plot.h"
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>   // std::max
#include <cstdio>      // std::printf

// ── Box-drawing helpers ───────────────────────────────────────────
static constexpr int  WIDTH  = 62;   // inner content width
static const char*    HL     = "â"; // U+2500 ─
static const char*    VL     = "â"; // U+2502 │
static const char*    TL     = "â"; // U+250C ┌
static const char*    TR     = "â"; // U+2510 ┐
static const char*    BL     = "â"; // U+2514 └
static const char*    BR     = "â"; // U+2518 ┘
static const char*    ML     = "â"; // U+251C ├
static const char*    MR     = "â¤"; // U+2524 ┤

static std::string hline(int w) {
    std::string s;
    for (int i = 0; i < w; ++i) s += HL;
    return s;
}
static void top_border()   { std::cout << TL << hline(WIDTH) << TR << '\n'; }
static void mid_border()   { std::cout << ML << hline(WIDTH) << MR << '\n'; }
static void bot_border()   { std::cout << BL << hline(WIDTH) << BR << '\n'; }
static void row(const std::string& text) {
    // Pad / truncate to WIDTH bytes (ASCII-safe).
    std::string t = text.substr(0, WIDTH);
    t += std::string(WIDTH - static_cast<int>(t.size()), ' ');
    std::cout << VL << t << VL << '\n';
}
static void blank() { row(""); }
// ── Screen layout ────────────────────────────────────────────────
static void draw_header() {
    top_border();
    row("  Aktienkurse  –  Hash-Table Stock Manager");
    mid_border();
}

static void draw_menu() {
    blank();
    row("  [1] ADD      Add a new stock");
    row("  [2] DEL      Delete a stock");
    row("  [3] IMPORT   Import NASDAQ CSV");
    row("  [4] SEARCH   Search by name or ticker");
    row("  [5] PLOT     ASCII price chart");
    row("  [6] LIST     List all stocks");
    row("  [7] SAVE     Save to file");
    row("  [8] LOAD     Load from file");
    row("  [9] QUIT");
    blank();
    mid_border();
    row("  Command: ");
    bot_border();
}

static void draw_status(const std::string& msg) {
    top_border();
    row("  " + msg);
    bot_border();
    std::cout << '\n';
}

static std::string prompt(const std::string& label) {
    top_border();
    row("  " + label);
    bot_border();
    std::string val;
    std::cout << "  > ";
    std::getline(std::cin, val);
    return val;
}
// ── Main ─────────────────────────────────────────────────────────
int main() {
    std::vector<std::unique_ptr<Stock>> owner;
    HashTable ht;

    while (true) {
        std::cout << "\033[2J\033[H";   // clear screen (ANSI)
        draw_header();
        draw_menu();
        std::cout << "  > ";
        std::string line;
        if (!std::getline(std::cin, line)) break;
        std::istringstream ss(line);
        std::string cmd; ss >> cmd;

        if (cmd == "9" || cmd == "QUIT") break;

        // ── ADD ──────────────────────────────────────────────────
        if (cmd == "1" || cmd == "ADD") {
            auto name   = prompt("Stock name:");
            auto wkn    = prompt("WKN:");
            auto ticker = prompt("Ticker:");
            try {
                auto s = std::make_unique<Stock>(name, wkn, ticker);
                ht.ht_insert(name,   s.get());
                ht.ht_insert(ticker, s.get());
                owner.push_back(std::move(s));
                draw_status("Added: " + name + "  (" + ticker + ")");
            } catch (const std::exception& e) {
                draw_status(std::string("Error: ") + e.what());
            }

        // ── DEL ──────────────────────────────────────────────────
        } else if (cmd == "2" || cmd == "DEL") {
            auto key = prompt("Name or ticker to delete:");
            if (Stock* st = ht.ht_lookup(key)) {
                ht.ht_delete(st->getName());
                ht.ht_delete(st->getTicker());
                owner.erase(
                    std::remove_if(owner.begin(), owner.end(),
                        [&](const auto& p){ return p.get() == st; }),
                    owner.end());
                draw_status("Deleted: " + key);
            } else { draw_status("Not found: " + key); }

        // ── IMPORT ───────────────────────────────────────────────
        } else if (cmd == "3" || cmd == "IMPORT") {
            auto key  = prompt("Name or ticker:");
            auto file = prompt("CSV file path:");
            if (Stock* st = ht.ht_lookup(key)) {
                try { st->importCSV(file); draw_status("Imported OK"); }
                catch (const std::exception& e)
                    { draw_status(std::string("Error: ") + e.what()); }
            } else { draw_status("Stock not found: " + key); }

        // ── SEARCH ───────────────────────────────────────────────
        } else if (cmd == "4" || cmd == "SEARCH") {
            auto key = prompt("Name or ticker:");
            if (const Stock* st = ht.ht_lookup(key)) {
                if (!st->hasHistory()) {
                    draw_status(st->getName() + "  –  no price data");
                } else {
                    const auto& e = st->latest();
                    draw_status(st->getName()
                        + "  close=" + std::to_string(e.close)
                        + "  vol="   + std::to_string(e.volume));
                }
            } else { draw_status("Not found: " + key); }

        // ── PLOT ─────────────────────────────────────────────────
        } else if (cmd == "5" || cmd == "PLOT") {
            auto key = prompt("Name or ticker:");
            if (const Stock* st = ht.ht_lookup(key))
                plotStock(*st);
            else draw_status("Not found: " + key);
            prompt("[press Enter to continue]");

        // ── LIST ─────────────────────────────────────────────────
        } else if (cmd == "6" || cmd == "LIST") {
            top_border();
            ht.listAll();
            bot_border();
            prompt("[press Enter to continue]");

        // ── SAVE ─────────────────────────────────────────────────
        } else if (cmd == "7" || cmd == "SAVE") {
            auto file = prompt("Save to file:");
            try { ht.save(file); draw_status("Saved to " + file); }
            catch (const std::exception& e)
                { draw_status(std::string("Error: ") + e.what()); }

        // ── LOAD ─────────────────────────────────────────────────
        } else if (cmd == "8" || cmd == "LOAD") {
            auto file = prompt("Load from file:");
            try { ht.load(file); draw_status("Loaded from " + file); }
            catch (const std::exception& e)
                { draw_status(std::string("Error: ") + e.what()); }

        } else { draw_status("Unknown command: " + cmd); }
    }  // while

    std::cout << "\033[2J\033[H";
    draw_status("Goodbye.");
}