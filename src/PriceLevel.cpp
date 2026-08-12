
#include "../include/PriceLevel.hpp"


PriceLevel::PriceLevel(int64_t _price) : price(_price), totalQuantity(0) {}

std::list<Order>::iterator PriceLevel::addOrder(Order& order) {
    totalQuantity += order.remQuantity;
    auto itr = orders.insert(orders.end(), order);
    return itr;
}

void PriceLevel::removeOrder(std::list<Order>::iterator& orderItr) {
    totalQuantity -= orderItr->remQuantity;    // Suppose in book, if we want to cancel an unfulfilled order, we need to remove's count from that level's totalQuantity 
    orders.erase(orderItr);
    
}

Order& PriceLevel::front() {
    return orders.front();
}

bool PriceLevel::isEmpty() const {
    return orders.empty();
}