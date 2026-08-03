#ifndef ENUMS_HPP
#define ENUMS_HPP

enum class Side  {
    BUY,
    SELL
};

enum class OrderType {
    LIMIT,      // Keep remainder in book
    MARKET      // Drop the remainder
};

#endif

