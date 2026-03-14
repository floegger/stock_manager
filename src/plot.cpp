#include "../include/plot.h"
#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

std::vector<std::string> plotStock ( const Stock &stock ) {
    std::vector<std::string> lines;
    const auto &history = stock.history();

    if ( history.empty() ) {
        lines.push_back ( "  No data available for: " + stock.getName() );
        return lines;
    }

    auto [ minIt, maxIt ] = std::minmax_element (
        history.begin(), history.end(),
        [] ( const PriceEntry &a, const PriceEntry &b ) { return a.close < b.close; } );

    double lo = minIt->close;
    double hi = maxIt->close;
    if ( hi == lo )
        hi = lo + 1.0;

    constexpr int ROWS = 10;
    const double step = ( hi - lo ) / ROWS;

    lines.push_back ( "  " + stock.getName() + " – Close Prices (" +
                      std::to_string ( static_cast<int> ( history.size() ) ) + " days)" );
    lines.push_back ( "" );

    for ( int r = ROWS; r >= 0; --r ) {
        double threshold = lo + r * step;
        char label[ 16 ];
        std::snprintf ( label, sizeof ( label ), "%9.2f | ", threshold );
        std::string line = label;
        for ( const auto &entry : history )
            line += ( entry.close >= threshold ? '*' : ' ' );
        lines.push_back ( line );
    }

    // x-axis rule
    std::string rule ( 12, ' ' );
    rule += std::string ( history.size(), '-' );
    lines.push_back ( rule );

    // oldest / newest label
    const int pad = static_cast<int> ( history.size() ) - 6 - 6;  // len("oldest")==len("newest")==6
    std::string axis = "  oldest";
    axis += std::string ( pad > 0 ? pad : 1, ' ' );
    axis += "newest";
    lines.push_back ( axis );

    return lines;
}
