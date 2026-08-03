
#ifndef ORDER_HPP
#define ORDER_HPP

#include "Enums.hpp"
#include "Utils.hpp"

class Order {
    public:
        static int id;
        int Oid;
        Side side;
        OrderType orderType;
        double price;
        int quantity;
        int remQuantity;
        long long  timeStamp;

    
        Order (Side _side, OrderType _orderType, double _price, int _quantity,
                int _remQuantity);  // Need to handle timestamp and id gen in cpp

        bool isFilled();

        // double getPrice();
        // int getQuantity();
        // int getRemQuantity();
        // long long orderTimeStamp();

};

#endif
