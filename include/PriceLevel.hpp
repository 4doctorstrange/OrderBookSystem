#ifndef PRICE_LEVEL_HPP
#define PRICE_LEVEL_HPP

#include <list>
#include "Order.hpp"

class PriceLevel {
    public:
        int64_t price;
        std::list<Order> orders;
        int totalQuantity;

        PriceLevel(int64_t _price); 
        std::list<Order>::iterator addOrder(Order& order);
        void removeOrder(std::list<Order>::iterator&  order);
        Order& front();
        bool isEmpty() const;


};

#endif