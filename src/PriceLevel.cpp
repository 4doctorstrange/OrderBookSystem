
#include "../include/PriceLevel.hpp"


PriceLevel::PriceLevel(double _price) : price(_price) {}

std::list<Order>::iterator PriceLevel::addOrder(Order& order) {
    // orders.push_back(order);

    totalQuantity += order.remQuantity;
    auto itr = orders.insert(orders.end(), order);
    return itr;
}

void PriceLevel::removeOrder(std::list<Order>::iterator& orderItr) {
    orders.erase(orderItr);
    
}

Order& PriceLevel::front() {
    return orders.front();
}

bool PriceLevel::isEmpty() {
    return orders.empty();
}