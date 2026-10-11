#include "modules/comfort/InventoryKeys.hpp"
#include <cassert>

int main() {
    std::array<int, 9> bindings{'1','2','3','4',16,'6','7','8','9'};
    assert(inventoryKeys::blocked(16, bindings, 'F'));
    assert(inventoryKeys::blocked('5', bindings, 'F'));
    assert(!inventoryKeys::blocked('F', bindings, 'F'));
    assert(!inventoryKeys::blocked(1, bindings, 'F'));
    assert(!inventoryKeys::blocked(2, bindings, 'F'));
    bindings[4] = 'F';
    assert(!inventoryKeys::blocked('F', bindings, 'F'));
    assert(inventoryKeys::blocked('F', bindings, 'G'));
    assert(!inventoryKeys::blocked(-99, bindings, 'F'));
    assert(!inventoryKeys::blocked(16, bindings, 'F'));
}
