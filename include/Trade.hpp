#ifndef TRADE_HPP
#define TRADE_HPP

#include "Utils.hpp"

class Trade {
    public:
        int buyOrderId;
        int sellOrderId;
        int64_t price;
        int quantity;
        long long  timeStamp;


        Trade (int _buyId, int _sellId, int64_t _price, int _quantity); // Need to handle timestamp

};

#endif
