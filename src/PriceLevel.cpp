
#include "../include/PriceLevel.hpp"


PriceLevel::PriceLevel(int64_t _price) : price(_price), totalQuantity(0), headIdx(-1), tailIdx(-1) {}

void PriceLevel::addOrder(std::vector<Order>& pool, int poolIdx) {
    auto& newOrder=  pool[poolIdx];
    totalQuantity += newOrder.remQuantity;
   
    // This level was empty, pool Idx becomes head and tail whose both next and prev are -1
    if (headIdx == -1 && tailIdx == -1) { 
        headIdx = poolIdx;
        
    } else {
        auto& lastOrderIdxOfLevel = pool[tailIdx];
        lastOrderIdxOfLevel.nextIdx = poolIdx;
        newOrder.prevIdx = tailIdx;
    }

    tailIdx = poolIdx;  // Update tail ptr to new order's index
}

void PriceLevel::removeOrder(std::vector<Order>& pool, int poolIdx) {
    auto& toBeDeletedOrder = pool[poolIdx];
    totalQuantity -= toBeDeletedOrder.remQuantity;    // Suppose in book, if we want to cancel an unfulfilled order, we need to remove's count from that level's totalQuantity 

    if (toBeDeletedOrder.prevIdx == -1 && toBeDeletedOrder.nextIdx == -1) {
        // current level's only node is about to get deleted
        headIdx = -1;
        tailIdx = -1;
    }
    else if (poolIdx == tailIdx) {
        // last node is getting deleted 
        tailIdx = toBeDeletedOrder.prevIdx;
        pool[tailIdx].nextIdx = -1;     // Make new tails next -1
    } else if (poolIdx == headIdx) {
        // head node is getting deleted 
        headIdx = toBeDeletedOrder.nextIdx;  // next node becomes head
        pool[headIdx].prevIdx = -1;         // new head's prev becomes -1;
    } else {
        // Middle node is getting deleted
        auto& prev = pool[toBeDeletedOrder.prevIdx];
        prev.nextIdx = toBeDeletedOrder.nextIdx;

        auto& next = pool[toBeDeletedOrder.nextIdx];
        next.prevIdx = toBeDeletedOrder.prevIdx;
    }
    
    // free the space occupied by toBeDeletedOrder;
    pool[poolIdx] = Order{};   
    
}

int PriceLevel::front() {
    return headIdx;
}

bool PriceLevel::isEmpty() const {
    return headIdx == -1;
}