#ifndef PRICE_LEVEL_HPP
#define PRICE_LEVEL_HPP

#include <list>
#include <vector>
#include "Order.hpp"

class PriceLevel {
    public:
        int64_t price;
        int totalQuantity;

        PriceLevel(int64_t _price); 
        void addOrder(std::vector<Order>& pool, int poolIdx);   // pool index where that order is added is added in pool
        void removeOrder(std::vector<Order>& pool, int poolIdx);
        int front();
        bool isEmpty() const;
        int headIdx;  // Addition after head;
        int tailIdx;  


};

#endif