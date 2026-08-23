
#include "../include/OrderBook.hpp"
#include <iostream>
#include <stdexcept>

OrderBook::OrderBook(int poolSize): PoolSize(poolSize)  {
    Bids.reserve(Max_Ticks);
    Asks.reserve(Max_Ticks);
    for (auto i = 0; i < Max_Ticks; i++) {
        Bids.emplace_back(MIN_TICK + i);    // level's price = its tick
        Asks.emplace_back(MIN_TICK + i);
    }
    bestAskIdx = -1;
    bestBidIdx = Max_Ticks;
    OrderPool.resize(PoolSize);
    FreeList.resize(PoolSize);
    for (int i = 0; i < PoolSize; i++) {
        FreeList[i] = i;
    }
};

bool OrderBook::checkIfOrderCanBeCompleted(Order& order) {
    auto cutOffPrice = order.price - MIN_TICK; // cutoff price;
    auto quantityRequested = order.remQuantity;

    if (order.side == Side::BUY) {
        for (int i = 0; i < Max_Ticks && i <= cutOffPrice; i++) {
            auto priceLevel = Asks[i];
            if (!priceLevel.isEmpty()) {
                quantityRequested -= priceLevel.totalQuantity;
            }
        }

    } else {
        for (int i = 0; i < Max_Ticks && i >= cutOffPrice; i++) {
            auto priceLevel = Bids[i];
            if (!priceLevel.isEmpty()) {
                quantityRequested -= priceLevel.totalQuantity;
            }
        }
    }
 
    if (quantityRequested > 0) return false;    //All Valid PRiceLevel can't complete order
    return true;
}

std::vector<Trade> OrderBook::addOrder(Order& order) {
    //order.price *= utils::tickMultiplier;   // Adjust price 
    auto trades = match(order);

    if (order.remQuantity && order.orderType == OrderType::LIMIT) {  // MArket and IOC will drop the remaining Order
        rest(order); // need to define;
    }

    return trades;
}

std::vector<Trade> OrderBook::match(Order& order) {
    std::vector<Trade>  trades;
    auto cutOffPrice = order.price - MIN_TICK; // cutoff price;
    bool isMarket = (order.orderType == OrderType::MARKET); // market ignores the price limit

    if (order.orderType == OrderType::FOK ) {
        if (!(checkIfOrderCanBeCompleted(order))) {
            return trades;
        }
    }
    

    // Upcoming order is to buy, we'll start with smallest Ask
    if (order.side == Side::BUY) {
        // auto it = Asks.begin();
        int Idx = bestAskIdx;
        
        while (Idx >= 0 && Idx < Max_Ticks && (isMarket || Idx <= cutOffPrice) && order.remQuantity) {    // we got atleast one pricelevel 
            auto& priceLevelList = Asks[Idx];
            // if Price level is empty , then continue
            if (priceLevelList.isEmpty()) {
                ++Idx;
                continue;
            }
            // auto priceOrderItr = priceLevelList.orders.begin();
            auto  priceOrderIdx = priceLevelList.headIdx;
            
            while (priceOrderIdx != -1 && order.remQuantity) {
                auto& orderFromPool = OrderPool[priceOrderIdx];
                int& quantityCouldBeFulfilled = orderFromPool.remQuantity;
                
                int fill = std::min(quantityCouldBeFulfilled, order.remQuantity);
                quantityCouldBeFulfilled -= fill;
                order.remQuantity -= fill;
                priceLevelList.totalQuantity -= fill;       // subtract the "fill" from total quantity of a level also

                Trade tr(order.Oid, orderFromPool.Oid, orderFromPool.price, fill );
                trades.push_back(tr);
                
                // Check if order in Ask is fullfilled or not
                if (orderFromPool.isFilled()) {
                    // delete current order
                    OrdersInBook.erase(orderFromPool.Oid);  // remove from All orders map
                    // priceOrderItr = priceLevelList.orders.erase(priceOrderItr);
                    auto temp = orderFromPool.nextIdx;
                    priceLevelList.removeOrder(OrderPool, priceOrderIdx);
                    FreeList.push_back(priceOrderIdx);   // ADD index back in pool;
                    priceOrderIdx = temp;
                    
                } else {
                    priceOrderIdx = orderFromPool.nextIdx;
                }
            }

            // If current Price level become empty and it was bestPrice, then we need to find next best
            if (priceLevelList.isEmpty() && priceLevelList.price - MIN_TICK == bestAskIdx) {
                bitPoolAsk.clear(Idx);  // Clear that bit representing that price
                getNextBestAskIdx();
            }

            ++Idx;  
        }  
        
    }

    // Upcoming order is Sell, we'll sell the order to higherst bidder
    else {
        // auto it = Bids.begin();
        int Idx = bestBidIdx;
        
        while (Idx >=0 && Idx < Max_Ticks && (isMarket ||  Idx >= cutOffPrice) && order.remQuantity) {    // atelast one bidder satisfies 
            auto& priceLevelList = Bids[Idx]; // fetch linked list orders
            // if Price level is empty , then continue
            if (priceLevelList.isEmpty()) {
                ++Idx;
                continue;
            }
            auto  priceOrderIdx = priceLevelList.headIdx;

            while (priceOrderIdx != -1 && order.remQuantity) {
                auto& orderFromPool = OrderPool[priceOrderIdx];
                int& quantityCouldBeFulfilled = orderFromPool.remQuantity;

                int fill = std::min(quantityCouldBeFulfilled, order.remQuantity);
                quantityCouldBeFulfilled -= fill;
                order.remQuantity -= fill;
                priceLevelList.totalQuantity -= fill;       // subtract the "fill" from total quantity of a level also
                Trade tr(orderFromPool.Oid, order.Oid, orderFromPool.price, fill );
                trades.push_back(tr);
                // Check if order in BIDS is fullfilled or not
                if (orderFromPool.isFilled()) {
                    // delete current order
                    auto temp = orderFromPool.nextIdx;
                    priceLevelList.removeOrder(OrderPool, priceOrderIdx);
                    FreeList.push_back(priceOrderIdx);   // ADD index back in Free pool;
                    priceOrderIdx = temp;
                    
                } else {
                    priceOrderIdx = orderFromPool.nextIdx;
                }
            }

            // If current Price level become empty and it was bestPrice, then we need to find next best
            if (priceLevelList.isEmpty() && priceLevelList.price - MIN_TICK == bestBidIdx) {
                bitPoolBid.clear(Idx);
                getNextBestBidIdx();
            }
            --Idx;
        }
    }

    return trades;
}


/* What is happeing here:
1- We are adding Order in BUY/SELL queue having some remaining Qunatities
2- We determine in which Queue to add order BUY/SELL, depending on order side
3- If the priceLevel of that order's price, doesn;t exist. We create one level of that price. ".first" returns the iterator to newly created level
4- we add order in that priceLevel list. The additon returns iterator where this order is being added.
5- This orderIterator will tell us where the new Order is being stored. handy in removal

*/
void OrderBook::rest(Order& order) {
    
    // get free slot from pool and set order
    int poolIdx = acquire();
    if (poolIdx == -1) {
        // TODO: If pool is exhausted, resize the pool size instead of throwing exception
        throw std::runtime_error("No objects available in pool");
    }
    OrderPool[poolIdx] = order;
    
    if (order.side == Side::SELL) {
        if (order.price - MIN_TICK < bestAskIdx ||  bestAskIdx == -1) {
            bestAskIdx = order.price - MIN_TICK;
        }
       
        Asks[order.price - MIN_TICK].addOrder(OrderPool, poolIdx);
        bitPoolAsk.set(order.price - MIN_TICK);       // Set that tick level in bit map
        
    } else {
        if (order.price - MIN_TICK > bestBidIdx ||  bestBidIdx == Max_Ticks) {
            bestBidIdx = order.price - MIN_TICK;
        }
        Bids[order.price - MIN_TICK].addOrder(OrderPool, poolIdx);
        bitPoolBid.set(order.price - MIN_TICK);
    }

    OrdersInBook[order.Oid] = poolIdx;
} 

/*
TODO: This function is not updated and will not compile, either remove or fix it
*/ 
// void OrderBook::cancelOrder(const int& id) {

//     // Check order in Bids
//     for (auto priceLevelItr = Bids.begin(); priceLevelItr != Bids.end(); ++priceLevelItr) {
//         auto& orderLists = priceLevelItr->second.orders;
//         for (auto orderItr = orderLists.begin(); orderItr != orderLists.end(); ++orderItr) {
//             if (orderItr->Oid == id) {
//                 orderItr = orderLists.erase(orderItr);
//                 return;
//             }
//         }
//     }

//     // check order in Asks
//     for (auto priceLevelItr = Asks.begin(); priceLevelItr != Asks.end(); ++priceLevelItr) {
//         auto& orderLists = priceLevelItr->second.orders;
//         for (auto orderItr = orderLists.begin(); orderItr != orderLists.end(); ++orderItr) {
//             if (orderItr->Oid == id) {
//                 orderItr = orderLists.erase(orderItr);
//                 break;
//             }
//         }
//     }
// }

bool OrderBook::optimalCancelOrder(const int& oid) {

    auto it = OrdersInBook.find(oid);
    if (it == OrdersInBook.end()) {
        return false;            // Cancel can;t happen;
    }
    
    auto& poolIdx = it->second;
    auto& orderToCancel = OrderPool[poolIdx];
    // order is in BUY
    if (orderToCancel.side == Side::BUY) {
        auto& priceLevel = Bids[orderToCancel.price - MIN_TICK];
        // auto& priceLevel = priceLevelItr;
        priceLevel.removeOrder(OrderPool, poolIdx);
        FreeList.push_back(poolIdx);   // ADD index back in pool;
        
        // If a Price level is empty remove that empty level from BIDS
        if (priceLevel.isEmpty() && bestBidIdx == orderToCancel.price - MIN_TICK) {

            // clear that price level from bit pool
            bitPoolBid.clear(bestBidIdx);
            // find new bestBidIdx as cuurent priceLevel is going to be empty;
            getNextBestBidIdx();
        }

    } else {
        auto& priceLevel = Asks[orderToCancel.price- MIN_TICK];
        priceLevel.removeOrder(OrderPool, poolIdx);
        FreeList.push_back(poolIdx);       // ADD index back in pool;

        // If a Price level is empty remove that empty level from Asks
        if (priceLevel.isEmpty() && bestAskIdx == orderToCancel.price - MIN_TICK) {
            
            // clear that price level from ask pool
            bitPoolAsk.clear(bestAskIdx);

            // find new bestAskIdx as cuurent priceLevel  is going to be empty;
            getNextBestAskIdx();
            
        }
    }
    // remove from raw;
    OrdersInBook.erase(it);
    return true;
}

std::optional<int> OrderBook::bestBid() {
    // Best bids will be present in non empty index
    if (bestBidIdx != Max_Ticks) {
        return bestBidIdx + MIN_TICK;
    }
    return std::nullopt;
}

std::optional<int> OrderBook::bestAsk() {
    // Best bids will be present in starting 
    if (bestAskIdx != -1) {
        return bestAskIdx + MIN_TICK;
    }
    return std::nullopt;
}

std::optional<int> OrderBook::spread() {
    auto best_bid = bestBid();
    auto best_ask = bestAsk();
    if (best_bid && best_ask) {
        return *best_ask - *best_bid;
    }
    return std::nullopt;
}


void OrderBook:: getNextBestBidIdx() {
    bestBidIdx =  bitPoolBid.find_first_from_left();

}

void OrderBook:: getNextBestAskIdx() {
    bestAskIdx = bitPoolAsk.find_first_from_right();

}

int OrderBook:: acquire() {
    int poolIdx = -1;
    if (!FreeList.empty()) {
        poolIdx = FreeList.back();
        FreeList.pop_back();
    }
    
    return poolIdx;
}