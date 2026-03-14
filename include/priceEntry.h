#ifndef PRICE_ENTRY_H
#define PRICE_ENTRY_H

#include <string>

struct PriceEntry {
    std::string date;
    double close = 0.0;
    long volume = 0;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
};

#endif
