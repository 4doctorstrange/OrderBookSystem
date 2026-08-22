
#include "../include/Order.hpp"

int Order::id = 0;

Order::Order() { price = -1;}     // Those order who are free or not in book. order in book must have a valid price

Order::Order(Side _side, OrderType _orderType, double _price, int _quantity, int _remQuantity)
                : side(_side), orderType(_orderType), price(static_cast<int64_t>(_price * utils::tickMultiplier) ), quantity(_quantity), remQuantity(_remQuantity) {
                    Oid = id++;
                    timeStamp = utils::getTimestamp();
                    nextIdx = -1;   // initialise with -1, will be updated while adding in book.
                    prevIdx = -1;     // initialise with -1, will be updated while adding in book.
                }

bool Order::isFilled() {
    if (remQuantity == 0) return true;
    return false;
}


