
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
        int64_t price;
        int quantity;
        int remQuantity;
        long long  timeStamp;
    
        Order (Side _side, OrderType _orderType, double _price, int _quantity,
                int _remQuantity);  // Need to handle timestamp and id gen in cpp

        bool isFilled();


};

#endif
