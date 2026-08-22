#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include "PriceLevel.hpp"
#include "Trade.hpp"
#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>
#include <optional>

class OrderBook {
    public: 
        // std::map<int64_t, PriceLevel, std::greater<int64_t> > Bids;  // Largest bids first to maximise profit
        // std::map<int64_t, PriceLevel> Asks;  // Smaller asks first to maximise profit
        std::unordered_map<int, int> OrdersInBook;    // <oid, poolIdx>
        OrderBook();

        std::vector<Trade> addOrder(Order& order); // will return all the trades this order has generated
        std::vector<Trade> match(Order& order);
        void cancelOrder(const int& oid);
        bool optimalCancelOrder(const int& oid);
        void rest(Order& order);
        bool checkIfOrderCanBeCompleted(Order& order);
        std::optional<int> bestBid();  // highest price buyer is willing to pay
        std::optional<int> bestAsk();  // Lowest price seller is accepting 
        std::optional<int> spread();  // difference between the above

        
        // Since our bench generator will be generating ticks in range [9900 - 10100], so we'll adjust MinTick and MaxTick accordingly
        /*
        TODO:  Handle testcases when tick is out this range;
        */
        static constexpr int64_t MIN_TICK = 9000;
        static constexpr int64_t MAX_TICK = 11000;
        size_t Max_Ticks = MAX_TICK - MIN_TICK + 1;  // 2001. => size of vector
        // Here each{tick - MIN_TICK} index represent a price level; 
        // suppose share price is 500.5 => 10010 Tick =>  10010 - 9000 => 1010 index in vector
        std::vector<PriceLevel> Bids;
        std::vector<PriceLevel> Asks;
        utils::BitPool2000 bitPoolAsk;
        utils::BitPool2000 bitPoolBid;
        int64_t bestBidIdx;
        int64_t bestAskIdx;
        void getNextBestBidIdx();
        void getNextBestAskIdx();
        
        std::vector<Order> OrderPool; // Object pool;
        int acquire();   // return a free slot;

};

#endif