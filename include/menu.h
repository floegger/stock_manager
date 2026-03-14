#ifndef MENU_H
#define MENU_H

#include "hash_table.h"
#include "stock.h"

#include <memory>
#include <vector>

using owner_t = std::vector<std::unique_ptr<Stock>>;

void run_menu ( HashTable &ht, owner_t &owner );

#endif // MENU_H
