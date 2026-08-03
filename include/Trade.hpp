#ifndef TRADE_HPP
#define TRADE_HPP

#include "Utils.hpp"

class Trade {
    public:
        int buyOrderId;
        int sellOrderId;
        double price;
        int quantity;
        long long  timeStamp;


        Trade (int _buyId, int _sellId, double _price, int _quantity); // Need to handle timestamp

};

#endif
