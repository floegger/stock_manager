#include "include/hash_table.h"
#include "include/menu.h"
#include "include/stock.h"

#include <memory>
#include <vector>

int main () {
    std::vector<std::unique_ptr<Stock>> owner;
    HashTable ht;

    run_menu ( ht, owner );
    return 0;
}
