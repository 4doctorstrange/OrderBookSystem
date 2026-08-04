
#include "../include/Trade.hpp"


Trade::Trade(int _buyId, int _sellId, int64_t _price, int _quantity): buyOrderId(_buyId), sellOrderId(_sellId),
     price(_price), quantity(_quantity), timeStamp(utils::getTimestamp())  {}
               