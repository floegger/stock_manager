#include "../include/tui.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

//  box-drawing constants
static const char *HL = "─";  // U+2500  (3 bytes, 1 col)
static const char *VL = "│";  // U+2502  (3 bytes, 1 col)
static const char *TL = "┌";  // U+250C
static const char *TR = "┐";  // U+2510
static const char *BL = "└";  // U+2514
static const char *BR = "┘";  // U+2518
static const char *ML = "├";  // U+251C
static const char *MR = "┤";  // U+2524

// UTF-8 helpers

static int utf8_display_width ( const std::string &s ) {
    int cols = 0;
    for ( unsigned char c : s )
        if ( ( c & 0xC0 ) != 0x80 )
            ++cols;
    return cols;
}

// Clip a UTF-8 string to at most max_cols display columns.
static std::string utf8_clip ( const std::string &s, int max_cols ) {
    int cols = 0;
    std::size_t i = 0;
    while ( i < s.size() ) {
        unsigned char c = static_cast<unsigned char> ( s[ i ] );
        int char_bytes = 1;
        if ( ( c & 0x80 ) == 0x00 )
            char_bytes = 1;
        else if ( ( c & 0xE0 ) == 0xC0 )
            char_bytes = 2;
        else if ( ( c & 0xF0 ) == 0xE0 )
            char_bytes = 3;
        else if ( ( c & 0xF8 ) == 0xF0 )
            char_bytes = 4;
        if ( cols + 1 > max_cols )
            break;
        cols += 1;
        i += static_cast<std::size_t> ( char_bytes );
    }
    return s.substr ( 0, i );
}

//  border / row

static std::string hline ( int w ) {
    std::string s;
    for ( int i = 0; i < w; ++i )
        s += HL;
    return s;
}

void top_border () { std::cout << TL << hline ( WIDTH ) << TR << '\n'; }
void mid_border () { std::cout << ML << hline ( WIDTH ) << MR << '\n'; }
void bot_border () { std::cout << BL << hline ( WIDTH ) << BR << '\n'; }

void row ( const std::string &text ) {
    std::string t = utf8_clip ( text, WIDTH );
    int w = utf8_display_width ( t );
    t += std::string ( static_cast<std::size_t> ( WIDTH - w ), ' ' );
    std::cout << VL << t << VL << '\n';
}

void blank () { row ( "" ); }

// menu content

static const std::vector<std::string> MENU_LINES = {
    "  [0] STATS    Hash-table diagnostics",
    "  [1] ADD      Add a new stock",
    "  [2] DEL      Delete a stock",
    "  [3] IMPORT   Import NASDAQ CSV",
    "  [4] SEARCH   Search by name or symbol",
    "  [5] PLOT     ASCII price chart",
    "  [6] LIST     List all stocks",
    "  [7] SAVE     Save to file",
    "  [8] LOAD     Load from file",
    "  [B] BATCH    Batch import from directory",
    "  [9] QUIT",
};

static void draw_body ( const std::vector<std::string> &output_lines, const std::string &status,
                        const std::string &prompt_label ) {
    std::cout << "\033[2J\033[H";  // clear screen, cursor to home

    // header
    top_border();
    row ( "  Aktienkurse  -  Hash-Table Stock Manager" );
    mid_border();

    // menu
    blank();
    for ( const auto &l : MENU_LINES )
        row ( l );
    blank();

    // status
    mid_border();
    if ( status.empty() )
        blank();
    else
        row ( "  " + status );

    // output panel
    if ( !output_lines.empty() ) {
        mid_border();
        for ( const auto &l : output_lines )
            row ( l );
    }

    // prompt label
    mid_border();
    row ( "  " + prompt_label );
    mid_border();
    // caller writes "│  > " and then reads input
}

//  Public API

std::string draw_screen ( const std::vector<std::string> &output_lines, const std::string &status ) {
    draw_body ( output_lines, status, "Command:" );

    std::cout << VL << "  > ";
    std::cout.flush();

    std::string input;
    std::getline ( std::cin, input );

    // close the box after the user presses Enter
    std::cout << '\n';
    bot_border();

    return input;
}

std::string input_screen ( const std::vector<std::string> &output_lines, const std::string &status,
                           const std::string &label ) {
    draw_body ( output_lines, status, label );

    std::cout << VL << "  > ";
    std::cout.flush();

    std::string input;
    std::getline ( std::cin, input );

    std::cout << '\n';
    bot_border();

    return input;
}

//  wrap lines

std::vector<std::string> wrap_lines ( const std::string &raw ) {
    std::vector<std::string> lines;
    std::istringstream ss ( raw );
    std::string line;
    while ( std::getline ( ss, line ) ) {
        while ( utf8_display_width ( line ) > WIDTH ) {
            std::string clipped = utf8_clip ( line, WIDTH );
            lines.push_back ( clipped );
            line = line.substr ( clipped.size() );
        }
        lines.push_back ( line );
    }
    return lines;
}
