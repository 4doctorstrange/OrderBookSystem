
#include "../include/Order.hpp"

int Order::id = 0;

Order::Order(Side _side, OrderType _orderType, double _price, int _quantity, int _remQuantity)
                : side(_side), orderType(_orderType), price(_price), quantity(_quantity), remQuantity(_remQuantity) {
                    Oid = id++;
                    timeStamp = utils::getTimestamp();
                }

bool Order::isFilled() {
    if (remQuantity == 0) return true;
    return false;
}


