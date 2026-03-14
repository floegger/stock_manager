// tui_ncurses.cpp  –  ncurses full-screen TUI
#include "HashTable.h"
#include "plot.h"
#include <ncurses.h>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

// ── Window geometry ───────────────────────────────────────────────
// Recomputed on SIGWINCH / KEY_RESIZE – kept in globals for simplicity.
static WINDOW* g_title  = nullptr;   // 1 line  – top
static WINDOW* g_menu   = nullptr;   // middle  – menu + output
static WINDOW* g_status = nullptr;   // 3 lines – bottom

static void destroy_windows() {
    if (g_title)  { delwin(g_title);  g_title  = nullptr; }
    if (g_menu)   { delwin(g_menu);   g_menu   = nullptr; }
    if (g_status) { delwin(g_status); g_status = nullptr; }
}

static void create_windows() {
    destroy_windows();
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    const int status_h = 3;
    const int menu_h   = rows - 2 - status_h; // 1 title + menu + status
    g_title  = newwin(1,        cols, 0,          0);
    g_menu   = newwin(menu_h,   cols, 1,          0);
    g_status = newwin(status_h, cols, rows - status_h, 0);
    scrollok(g_menu, TRUE);
    keypad(g_status, TRUE);
}
// ── Drawing helpers ───────────────────────────────────────────────
static void draw_title() {
    wattron(g_title, A_REVERSE);
    int cols = getmaxx(g_title);
    std::string title = "  Aktienkurse  –  Hash-Table Stock Manager";
    title += std::string(cols - static_cast<int>(title.size()), ' ');
    mvwprintw(g_title, 0, 0, "%s", title.c_str());
    wattroff(g_title, A_REVERSE);
    wrefresh(g_title);
}

static void draw_menu_items() {
    werase(g_menu);
    mvwprintw(g_menu, 1, 2, "[1] ADD      Add a new stock");
    mvwprintw(g_menu, 2, 2, "[2] DEL      Delete a stock");
    mvwprintw(g_menu, 3, 2, "[3] IMPORT   Import NASDAQ CSV");
    mvwprintw(g_menu, 4, 2, "[4] SEARCH   Search by name or ticker");
    mvwprintw(g_menu, 5, 2, "[5] PLOT     ASCII price chart");
    mvwprintw(g_menu, 6, 2, "[6] LIST     List all stocks");
    mvwprintw(g_menu, 7, 2, "[7] SAVE     Save to file");
    mvwprintw(g_menu, 8, 2, "[8] LOAD     Load from file");
    mvwprintw(g_menu, 9, 2, "[9] QUIT");
    wrefresh(g_menu);
}

static void set_status(const std::string& msg) {
    werase(g_status);
    mvwprintw(g_status, 0, 0, "%s", std::string(getmaxx(g_status), '-').c_str());
    mvwprintw(g_status, 1, 2, "%s", msg.c_str());
    wrefresh(g_status);
}

// Prompt in the status window; returns the entered string.
static std::string get_input(const std::string& label) {
    werase(g_status);
    mvwprintw(g_status, 0, 0, "%s", std::string(getmaxx(g_status), '-').c_str());
    mvwprintw(g_status, 1, 2, "%s", label.c_str());
    wrefresh(g_status);
    char buf[256] = {};
    echo();
    mvwgetnstr(g_status, 2, 2, buf, sizeof(buf) - 1);
    noecho();
    return std::string(buf);
}
// ── main ──────────────────────────────────────────────────────────
int main() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    start_color();
    use_default_colors();

    create_windows();
    draw_title();

    std::vector<std::unique_ptr<Stock>> owner;
    HashTable ht;

    while (true) {
        draw_menu_items();
        std::string cmd = get_input("Command (1-9):");

        // Handle terminal resize.
        if (cmd.empty() && is_term_resized(LINES, COLS)) {
            resize_term(0, 0);
            create_windows();
            draw_title();
            continue;
        }

        if (cmd == "9" || cmd == "QUIT") break;

        // ── ADD ──────────────────────────────────────────────────
        if (cmd == "1" || cmd == "ADD") {
            auto name   = get_input("Stock name:");
            auto wkn    = get_input("WKN:");
            auto ticker = get_input("Ticker:");
            try {
                auto s = std::make_unique<Stock>(name, wkn, ticker);
                ht.ht_insert(name,   s.get());
                ht.ht_insert(ticker, s.get());
                owner.push_back(std::move(s));
                set_status("Added: " + name + "  (" + ticker + ")");
            } catch (const std::exception& e) {
                set_status(std::string("Error: ") + e.what());
            }

        // ── DEL ──────────────────────────────────────────────────
        } else if (cmd == "2" || cmd == "DEL") {
            auto key = get_input("Name or ticker to delete:");
            if (Stock* st = ht.ht_lookup(key)) {
                ht.ht_delete(st->getName());
                ht.ht_delete(st->getTicker());
                owner.erase(
                    std::remove_if(owner.begin(), owner.end(),
                        [&](const auto& p){ return p.get() == st; }),
                    owner.end());
                set_status("Deleted: " + key);
            } else { set_status("Not found: " + key); }

        // ── IMPORT ───────────────────────────────────────────────
        } else if (cmd == "3" || cmd == "IMPORT") {
            auto key  = get_input("Name or ticker:");
            auto file = get_input("CSV file path:");
            if (Stock* st = ht.ht_lookup(key)) {
                try { st->importCSV(file); set_status("Imported OK"); }
                catch (const std::exception& e)
                    { set_status(std::string("Error: ") + e.what()); }
            } else { set_status("Stock not found: " + key); }

        // ── SEARCH ───────────────────────────────────────────────
        } else if (cmd == "4" || cmd == "SEARCH") {
            auto key = get_input("Name or ticker:");
            if (const Stock* st = ht.ht_lookup(key)) {
                if (!st->hasHistory())
                    set_status(st->getName() + "  –  no price data");
                else {
                    const auto& e = st->latest();
                    set_status(st->getName()
                        + "  close=" + std::to_string(e.close)
                        + "  vol="   + std::to_string(e.volume));
                }
            } else { set_status("Not found: " + key); }

        // ── PLOT ─────────────────────────────────────────────────
        // ncurses owns stdout; use def_prog_mode/endwin/reset cycle.
        } else if (cmd == "5" || cmd == "PLOT") {
            auto key = get_input("Name or ticker:");
            if (const Stock* st = ht.ht_lookup(key)) {
                def_prog_mode(); endwin();
                plotStock(*st);
                std::cout << "\n[press Enter]";
                std::cin.ignore(1024, '\n');
                reset_prog_mode(); refresh();
                create_windows(); draw_title();
            } else { set_status("Not found: " + key); }

        // ── LIST ─────────────────────────────────────────────────
        } else if (cmd == "6" || cmd == "LIST") {
            def_prog_mode(); endwin();
            ht.listAll();
            std::cout << "\n[press Enter]";
            std::cin.ignore(1024, '\n');
            reset_prog_mode(); refresh();
            create_windows(); draw_title();

        // ── SAVE ─────────────────────────────────────────────────
        } else if (cmd == "7" || cmd == "SAVE") {
            auto file = get_input("Save to file:");
            try { ht.save(file); set_status("Saved to " + file); }
            catch (const std::exception& e)
                { set_status(std::string("Error: ") + e.what()); }

        // ── LOAD ─────────────────────────────────────────────────
        } else if (cmd == "8" || cmd == "LOAD") {
            auto file = get_input("Load from file:");
            try { ht.load(file); set_status("Loaded from " + file); }
            catch (const std::exception& e)
                { set_status(std::string("Error: ") + e.what()); }

        } else { set_status("Unknown command: " + cmd); }
    }  // while

    destroy_windows();
    endwin();
}