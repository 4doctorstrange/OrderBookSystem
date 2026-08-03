#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include "PriceLevel.hpp"
#include "Trade.hpp"
#include <map>
#include <unordered_map>
#include <vector>

class OrderBook {
    public: 
        std::map<double, PriceLevel, std::greater<double> > Bids;  // Largest bids first to maximise profit
        std::map<double, PriceLevel> Asks;  // Smaller asks first to maximise profit
        std::unordered_map<int, std::list<Order>::iterator> OrdersInBook;
        OrderBook();

        std::vector<Trade> addOrder(Order& order); // will return all the trades this order has generated
        std::vector<Trade> match(Order& order);
        void cancelOrder(const int& oid);
        void optimalCancelOrder(const int& oid);
        void rest(Order& order);
};

#endif