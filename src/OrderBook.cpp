
#include "../include/OrderBook.hpp"
#include <iostream>

OrderBook::OrderBook() {};

bool OrderBook::checkIfOrderCanBeCompleted(Order& order) {
    auto cutOffPrice = order.price; // cutoff price;
    auto quantityRequested = order.remQuantity;

    if (order.side == Side::BUY) {
        auto it = Asks.begin();
        while (it != Asks.end() && it->first <= cutOffPrice && quantityRequested) {
            quantityRequested -= it->second.totalQuantity;
            ++it;
        }
    } else {
        auto it = Bids.begin();
        while (it != Bids.end() && it->first >= cutOffPrice && quantityRequested) {
            quantityRequested -= it->second.totalQuantity;
            ++it;
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
    auto cutOffPrice = order.price; // cutoff price;
    bool isMarket = (order.orderType == OrderType::MARKET); // market ignores the price limit

    if (order.orderType == OrderType::FOK ) {
        if (!(checkIfOrderCanBeCompleted(order))) {
            return trades;
        }
    }
    

    // Upcoming order is to buy, we'll start with smallest Ask
    if (order.side == Side::BUY) {
        auto it = Asks.begin();
        
        while (it != Asks.end() && (isMarket || it->first <= cutOffPrice) && order.remQuantity) {    // we got atleast one pricelevel 
            auto& priceLevelList = it->second;
            auto priceOrderItr = priceLevelList.orders.begin();

            while (priceOrderItr != priceLevelList.orders.end() && order.remQuantity) {
                int& quantityCouldBeFulfilled = priceOrderItr->remQuantity;
                
                int fill = std::min(quantityCouldBeFulfilled, order.remQuantity);
                quantityCouldBeFulfilled -= fill;
                order.remQuantity -= fill;
                priceLevelList.totalQuantity -= fill;       // subtract the "fill" from total quantity of a level also

                Trade tr(order.Oid, priceOrderItr ->Oid, priceOrderItr->price, fill );
                trades.push_back(tr);
                
                // Check if order in Ask is fullfilled or not
                if (priceOrderItr->isFilled()) {
                    // delete current order
                    priceOrderItr = priceLevelList.orders.erase(priceOrderItr);
                } else {
                    ++priceOrderItr;
                }
            }

            if (it->second.isEmpty()) {  // Remove a prive level, if all the orders are filled
                it = Asks.erase(it);
            } else {
                ++it;
            }
            
        }  
        
    }

    // Upcoming order is Sell, we'll sell the order to higherst bidder
    else {
        auto it = Bids.begin();
        while (it != Bids.end() && (isMarket || it -> first >= cutOffPrice) && order.remQuantity) {    // atelast one bidder satisfies 
            auto& priceLevelList = it ->second; // fetch linked list orders
            auto priceOrderItr = priceLevelList.orders.begin();
            while (priceOrderItr != priceLevelList.orders.end() && order.remQuantity) {
                int& quantityCouldBeFulfilled = priceOrderItr->remQuantity;
                int fill = std::min(quantityCouldBeFulfilled, order.remQuantity);
                quantityCouldBeFulfilled -= fill;
                order.remQuantity -= fill;
                priceLevelList.totalQuantity -= fill;       // subtract the "fill" from total quantity of a level also
                Trade tr(priceOrderItr -> Oid, order.Oid, priceOrderItr->price, fill );
                trades.push_back(tr);
                // Check if order in BIDS is fullfilled or not
                if (priceOrderItr->isFilled()) {
                    // delete current order
                    priceOrderItr = priceLevelList.orders.erase(priceOrderItr);
                } else {
                    ++priceOrderItr;
                }
            }
            if (it->second.isEmpty()) { // Remove a prive level, if all the orders are filled
                it = Bids.erase(it);
            } else {
                ++it;
            }
            
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
    
    std::list<Order>::iterator objectItr;
    if (order.side == Side::SELL) {
        auto itr = Asks.find(order.price);
        if (itr == Asks.end()) {
            itr = Asks.emplace(order.price, PriceLevel(order.price)).first;
        } 
        objectItr = itr->second.addOrder(order);
        
    } else {
        auto itr = Bids.find(order.price);
        if (itr == Bids.end()) {
            itr = Bids.emplace(order.price, PriceLevel(order.price)).first;
        } 
        objectItr = itr->second.addOrder(order);
    }

    OrdersInBook[order.Oid] = objectItr;
} 

void OrderBook::cancelOrder(const int& id) {

    // Check order in Bids
    for (auto priceLevelItr = Bids.begin(); priceLevelItr != Bids.end(); ++priceLevelItr) {
        auto& orderLists = priceLevelItr->second.orders;
        for (auto orderItr = orderLists.begin(); orderItr != orderLists.end(); ++orderItr) {
            if (orderItr->Oid == id) {
                orderItr = orderLists.erase(orderItr);
                return;
            }
        }
    }

    // check order in Asks
    for (auto priceLevelItr = Asks.begin(); priceLevelItr != Asks.end(); ++priceLevelItr) {
        auto& orderLists = priceLevelItr->second.orders;
        for (auto orderItr = orderLists.begin(); orderItr != orderLists.end(); ++orderItr) {
            if (orderItr->Oid == id) {
                orderItr = orderLists.erase(orderItr);
                break;
            }
        }
    }
}

void OrderBook::optimalCancelOrder(const int& oid) {
    auto& orderItr = OrdersInBook[oid];
    
    // order is in BUY
    if (orderItr->side == Side::BUY) {
        auto priceLevelItr = Bids.find(orderItr -> price);
        auto& priceLevel = priceLevelItr->second;
        priceLevel.removeOrder(orderItr);
        
        // If a Price level is empty remove that empty level from BIDS
        if (priceLevel.isEmpty()) {
            Bids.erase(priceLevelItr);
        }

    } else {
        auto priceLevelItr = Asks.find(orderItr -> price);
        auto& priceLevel = priceLevelItr->second;
        priceLevel.removeOrder(orderItr);

        // If a Price level is empty remove that empty level from Asks
        if (priceLevel.isEmpty()) {
           std::cout << "HITTING" << std::endl;
            Asks.erase(priceLevelItr);
        }
    }
    // remove from raw;
    OrdersInBook.erase(oid);
}