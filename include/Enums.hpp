#ifndef ENUMS_HPP
#define ENUMS_HPP

enum class Side  {
    BUY,
    SELL
};

enum class OrderType {
    LIMIT,      // Keep remainder in book
    MARKET,     // Drop the remainder
    FOK,        // Fill Or Kill -> Either complete the full order at given price or Don't 
    IOC,        // Immediate or Cancel -> Fill whatever can at given price, drop the rest
};

#endif

