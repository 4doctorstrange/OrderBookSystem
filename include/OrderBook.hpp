#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include "PriceLevel.hpp"
#include "Trade.hpp"
#include <map>
#include <unordered_map>
#include <vector>
#include <optional>

class OrderBook {
    public: 
        std::map<int64_t, PriceLevel, std::greater<int64_t> > Bids;  // Largest bids first to maximise profit
        std::map<int64_t, PriceLevel> Asks;  // Smaller asks first to maximise profit
        std::unordered_map<int, std::list<Order>::iterator> OrdersInBook;
        OrderBook();

        std::vector<Trade> addOrder(Order& order); // will return all the trades this order has generated
        std::vector<Trade> match(Order& order);
        void cancelOrder(const int& oid);
        void optimalCancelOrder(const int& oid);
        void rest(Order& order);
        bool checkIfOrderCanBeCompleted(Order& order);
        std::optional<int> bestBid();  // highest price buyer is willing to pay
        std::optional<int> bestAsk();  // Lowest price seller is accepting 
        std::optional<int> spread();  // difference between the above
};

#endif