#include <gtest/gtest.h>
#include "../include/OrderBook.hpp"
#include <optional>

using namespace std;
// helper: build a limit order (remQuantity starts equal to quantity)
static Order limit(Side s, double price, int qty) {
    return Order(s, OrderType::LIMIT, price, qty, qty);
}
static Order market(Side s, int qty) {
    return Order(s, OrderType::MARKET, 0.0, qty, qty);
}

// Prices are stored in ticks (price * tickMultiplier); convert a human price
// into the tick value the engine actually stores/returns.
static int64_t ticksOf(double price) {
    return static_cast<int64_t>(price * utils::tickMultiplier);
}


TEST(Basictest, EmptyBookTest) {
    OrderBook book;
    auto val = book.bestBid();

    EXPECT_EQ(val, std::nullopt);
}


// Scenario 1: two limits rest (no cross), then a buy that fully fills one.
TEST(LimitMatch, RestThenFullFill) {
    OrderBook book;

    // --- 1) SELL 50 @ 101 rests: no trade, one ask level ---
    auto a = limit(Side::SELL, 101.00, 50);
    auto res = book.addOrder(a);
    EXPECT_TRUE(res.empty());                 // nothing to cross -> no trades
    EXPECT_EQ(book.Asks.size(), 1u);

    // --- 2) BUY 100 @ 100 rests: below the ask, no cross ---
    auto b = limit(Side::BUY, 100.00, 100);
    res = book.addOrder(b);
    EXPECT_TRUE(res.empty());
    EXPECT_EQ(book.Bids.size(), 1u);

    // --- 3) BUY 50 @ 101 crosses and fully fills ask 'a' ---
    auto c = limit(Side::BUY, 101.00, 50);
    res = book.addOrder(c);

    // ASSERT (not EXPECT) on the count: if it's wrong, stop before indexing res[0].
    ASSERT_EQ(res.size(), 1u);
    const Trade& t = res[0];
    EXPECT_EQ(t.quantity, 50);
    EXPECT_EQ(t.price, ticksOf(101.00));      // exec price = resting ask's price
    EXPECT_EQ(t.buyOrderId, c.Oid);           // aggressor = the incoming buy
    EXPECT_EQ(t.sellOrderId, a.Oid);          // maker = the resting sell

    // --- 4) resulting book state ---
    EXPECT_EQ(book.Asks.size(), 0u);          // ask level fully consumed & removed
    ASSERT_EQ(book.Bids.size(), 1u);          // the untouched bid still rests
    EXPECT_EQ(book.bestBid(), ticksOf(100.00));
    // the resting bid's quantity was never touched
    EXPECT_EQ(book.Bids.begin()->second.orders.front().remQuantity, 100);
}

// Scenario 2: sweep multiple ask levels (price priority).
TEST(LIMIT_TEST, SweepPricePriorityTest) {
    OrderBook book;
    auto a = limit(Side::SELL, 101.70, 50);
    auto res = book.addOrder(a);
    EXPECT_TRUE(res.empty()); 
    EXPECT_EQ(book.Asks.size(), 1u);   

    auto b = limit(Side::SELL, 101.05, 50);
    res = book.addOrder(b);
    EXPECT_TRUE(res.empty());
    EXPECT_EQ(book.Asks.size(), 2u);   


    auto c = limit(Side::BUY, 102.00, 70);     // takes 50@101.05 then 20@101.70
    res = book.addOrder(c);
    // Assert as trade is done
    ASSERT_EQ(res.size(), 2u);

    // verify each trade;
    const Trade& t1 = res[0];
    EXPECT_EQ(t1.quantity, 50);
    EXPECT_EQ(t1.price, ticksOf(101.05));
    EXPECT_EQ(t1.buyOrderId, c.Oid);
    EXPECT_EQ(t1.sellOrderId, b.Oid);

    const Trade&t2  = res[1];
    EXPECT_EQ(t2.quantity, 20);
    EXPECT_EQ(t2.price, ticksOf(101.70));
    EXPECT_EQ(t2.buyOrderId, c.Oid);
    EXPECT_EQ(t2.sellOrderId, a.Oid);

    // verify book;
    EXPECT_EQ(book.Asks.size(), 1u);
    EXPECT_EQ(book.Bids.size(), 0u);          // bids level fully consumed & removed
    EXPECT_EQ(book.bestAsk(), ticksOf(101.70)); 
    EXPECT_EQ(book.Asks.begin()->second.totalQuantity, 30);  // (50 + 50 - 70)
}

// Scenario 3: FIFO / time priority at the same price.
TEST(LIMIT_TEST, FIFO_TIME_PRIORITY) {
    OrderBook book;
    auto a = limit(Side::SELL, 101.00, 50);    // arrives first
    auto res = book.addOrder(a);
    EXPECT_TRUE(res.empty()); 
    EXPECT_EQ(book.Asks.size(), 1u);   

    auto b = limit(Side::SELL, 101.00, 30);    // arrives later
    res = book.addOrder(b);
    EXPECT_TRUE(res.empty());
    EXPECT_EQ(book.Asks.size(), 1u);     // only 1 level of price 101.00


    auto c = limit(Side::BUY, 101.00, 60);     // fills #a fully (50), then 10 of #b
    res = book.addOrder(c);
    // Assert as trade is done
    ASSERT_EQ(res.size(), 2u);

    // verify each trade;
    const Trade& t1 = res[0];
    EXPECT_EQ(t1.quantity, 50);
    EXPECT_EQ(t1.price, ticksOf(101.00));
    EXPECT_EQ(t1.buyOrderId, c.Oid);
    EXPECT_EQ(t1.sellOrderId, a.Oid);  // a's id

    const Trade&t2  = res[1];
    EXPECT_EQ(t2.quantity, 10);
    EXPECT_EQ(t2.price, ticksOf(101.00));
    EXPECT_EQ(t2.buyOrderId, c.Oid);
    EXPECT_EQ(t2.sellOrderId, b.Oid);

    // verify book;
    EXPECT_EQ(book.Asks.size(), 1u);        // order B
    EXPECT_EQ(book.Bids.size(), 0u);          // bids level fully consumed & removed
    EXPECT_EQ(book.bestAsk(), ticksOf(101.00)); 
    EXPECT_EQ(book.Asks.begin()->second.totalQuantity, 20);  // (50 + 30 - 60)
}

 // Scenario 4: partial fill, remainder rests.
TEST(LIMIT_TEST, PARTIAL_FILL) {
    OrderBook book;
    auto a = limit(Side::SELL, 101.00, 30);
    auto res = book.addOrder(a);
    EXPECT_TRUE(res.empty()); 
    EXPECT_EQ(book.Asks.size(), 1u);  

    auto c = limit(Side::BUY, 101.00, 50);     // fills 30, 20 rests as a bid
    res = book.addOrder(c);
    ASSERT_EQ(res.size(), 1u);  // 1 trade will happen

    // verify trade;
    const Trade& t1 = res[0];
    EXPECT_EQ(t1.quantity, 30);     //only these were availbale in ASKS
    EXPECT_EQ(t1.price, ticksOf(101.00));
    EXPECT_EQ(t1.buyOrderId, c.Oid);
    EXPECT_EQ(t1.sellOrderId, a.Oid);  // a's id

    // verify book;
    EXPECT_EQ(book.Asks.size(), 0u);        // NO SELL Order
    EXPECT_EQ(book.Bids.size(), 1u);          // The rest order from C
    EXPECT_EQ(book.bestBid(), ticksOf(101.00)); 
    EXPECT_EQ(book.Bids.begin()->second.totalQuantity, 20);  // (30-50)
    EXPECT_EQ(book.Bids.begin()->second.orders.front().Oid, c.Oid);   // The only order in BIDS is order c
}

// Scenario 5: market order - fills best, leftover dropped (not rested).
TEST(MARKET_TEST,  MARKET_ORDER) {
    OrderBook book;
    auto a = limit(Side::SELL, 101.00, 50);
    auto res = book.addOrder(a);
    EXPECT_TRUE(res.empty()); 
    EXPECT_EQ(book.Asks.size(), 1u); 

    auto c = market(Side::BUY, 80);            // fills 50, leftover 30 dropped
    res = book.addOrder(c);
    ASSERT_EQ(res.size(), 1u);  // 1 trade will happen

    const Trade& t1 = res[0];
    EXPECT_EQ(t1.quantity, 50);     //only 50 were availbale in ASKS
    EXPECT_EQ(t1.price, ticksOf(101.00));
    EXPECT_EQ(t1.buyOrderId, c.Oid);
    EXPECT_EQ(t1.sellOrderId, a.Oid);  // a's id

    // verify book;
    EXPECT_EQ(book.Asks.size(), 0u);        // NO SELL Order
    EXPECT_EQ(book.Bids.size(), 0u);        //  No BUY ORDER AS 30 was dropped
    EXPECT_EQ(book.bestBid(), std::nullopt); 
    
    auto d = market(Side::SELL, 30); 
    res = book.addOrder(d);
    ASSERT_EQ(res.size(), 0u);  // No trade will happen

    // verify book;
    EXPECT_EQ(book.Asks.size(), 0u);  // STILL EMPTY AS complete Sell order was dropped as there was no BUY order 
}

// Scenario 6: cancel a resting order, then confirm matching skips it.
TEST(CANCEL_ORDER, CANCEL_REST) {
    OrderBook book;
    auto a = limit(Side::SELL, 101.00, 50);
    auto res = book.addOrder(a);
    EXPECT_TRUE(res.empty());

    auto b = limit(Side::SELL, 101.00, 30); 
    res = book.addOrder(b);
    EXPECT_TRUE(res.empty());

    auto c = limit(Side::SELL, 102.00, 40); 
    res = book.addOrder(c);
    EXPECT_TRUE(res.empty());

    EXPECT_EQ(book.Asks.size(), 2u) ;// 2 price level {101, 102};
    EXPECT_EQ(book.Asks.begin()->second.orders.size(), 2u); // 2 orders in 101 level
    EXPECT_EQ(book.Asks.begin()->second.totalQuantity, 80); // (101 : 50 + 30)

    book.optimalCancelOrder(a.Oid);
    EXPECT_EQ(book.Asks.size(), 2u); // Still 2 price level
    EXPECT_EQ(book.Asks.begin()->second.orders.size(), 1u);  // 1 order left in 101 level
    EXPECT_EQ(book.Asks.begin()->second.totalQuantity, 30); // (101 :  30)
}

// Scenario 7: IOC order — partial fill, remainder dropped (NOT rested)
TEST(IOC, IOC_PARTIAL_FILL) {
    OrderBook book;
    auto a = limit(Side::SELL, 101.00, 50);
    auto res = book.addOrder(a);

    auto b = limit(Side::SELL, 102.00, 30); 
    res = book.addOrder(b);
    EXPECT_TRUE(res.empty());

    EXPECT_EQ(book.Asks.begin()->first,ticksOf(101.00));  // 101 is first level

    // IOC BUY 70 @ 101: only 50 available at <=101, fills 50, remaining 20 DROPPED
    auto c = Order(Side::BUY, OrderType::IOC, 101.50, 70, 70);
    res = book.addOrder(c);

    ASSERT_EQ(res.size(), 1u);   // 1 trade

    // verify trade
    const Trade& t1 = res[0];
    EXPECT_EQ(t1.quantity, 50); 
    EXPECT_EQ(t1.price, ticksOf(101.00));
    EXPECT_EQ(t1.buyOrderId, c.Oid);
    EXPECT_EQ(t1.sellOrderId, a.Oid);  // a's id

     // verify book;
    EXPECT_EQ(book.Asks.size(), 1u);
    EXPECT_EQ(book.bestAsk(), ticksOf(102.00));    // level 101 is exhausted, 102 is only ask left
    EXPECT_EQ(book.Asks.begin()->first, ticksOf(102.00));   // 102 becomes 1st level
    EXPECT_EQ(book.Asks.begin()->second.orders.front().Oid, b.Oid);  // Order B is only one sitting there
}

// Scenario 8: IOC fully filled
TEST(IOC, IOC_FULLY_FILLED) {
    OrderBook book;
    auto a = limit(Side::SELL, 101, 50);
    auto res = book.addOrder(a);

    EXPECT_EQ(book.Asks.begin()->second.totalQuantity, 50);  // 50 quantiyt in Aks
    auto c = Order(Side::BUY, OrderType::IOC, 101, 30, 30);
    res = book.addOrder(c);

    ASSERT_EQ(res.size(), 1u);   // 1 trade
    const Trade& t1 = res[0];
    EXPECT_EQ(t1.quantity, 30); 
    EXPECT_EQ(t1.price, ticksOf(101.00));
    EXPECT_EQ(t1.buyOrderId, c.Oid);
    EXPECT_EQ(t1.sellOrderId, a.Oid);  // a's id

    // verify book;
    EXPECT_EQ(book.Asks.size(), 1u); 
    EXPECT_EQ(book.Bids.size(), 0u);  // No bids as c was fully filled
    EXPECT_EQ(book.Asks.begin()->second.totalQuantity, 20); // 20 is still left in Asks, which was 50
}

// Scenario 9: FOK rejected — not enough quantity
TEST(FOK, FOK_REJECTED_NOT_ENOUGH_QUANTITY) {
    OrderBook book;

    auto a = limit(Side::SELL, 101, 50);
    auto res = book.addOrder(a);

    auto c = Order(Side::BUY, OrderType::FOK, 101, 80, 80);
    res = book.addOrder(c);

    ASSERT_EQ(res.size(), 0u); // No trade happened as we didn;t have enough in Asks, fell short by 30
    EXPECT_EQ(book.Asks.size(), 1u); // sell order still there
    EXPECT_EQ(book.Bids.size(), 0u); // 0 Bids due to FOK
}

// Scenario 10: FOK accepted — exact quantity available
TEST(FOK, FOK_ACCEPTED) {
    OrderBook book;

    auto a = limit(Side::SELL, 101, 50);
    auto res = book.addOrder(a);

    auto b = limit(Side::SELL, 101, 30);
    res = book.addOrder(b);

    auto c = Order(Side::BUY, OrderType::FOK, 101, 80, 80);
    res = book.addOrder(c);

    ASSERT_EQ(res.size(), 2u);   // 2 trade

    // verify trade
    const Trade& t1 = res[0];
    EXPECT_EQ(t1.quantity, 50); 
    EXPECT_EQ(t1.price, ticksOf(101.00));
    EXPECT_EQ(t1.buyOrderId, c.Oid);
    EXPECT_EQ(t1.sellOrderId, a.Oid);  // a's

    const Trade& t2 = res[1];
    EXPECT_EQ(t2.quantity, 30); 
    EXPECT_EQ(t2.price, ticksOf(101.00));
    EXPECT_EQ(t2.buyOrderId, c.Oid);
    EXPECT_EQ(t2.sellOrderId, b.Oid);  // b's id

    // verify book
    EXPECT_EQ(book.Asks.size(), 0u); // 0 Asks left
    EXPECT_EQ(book.Bids.size(), 0u); // 0 Bids left
}

// Scenario 11: FOK rejected — price limit exceeded
TEST(FOK, FOK_REJECTED_PRICE_LIMIT) {
    OrderBook book;

    auto a = limit(Side::SELL, 101, 50);
    auto res = book.addOrder(a);

    auto b = limit(Side::SELL, 103, 30);
    res = book.addOrder(b);

    EXPECT_EQ(book.Asks.size(), 2u);
    auto it = book.Asks.begin();
    EXPECT_EQ(it->second.totalQuantity + (++it)->second.totalQuantity, 80);

    // FOK BUY 80 @ 102: need 80, only 50 at <=102 -> REJECT
    auto c = Order(Side::BUY, OrderType::FOK, 102, 80, 80);
    res = book.addOrder(c); 

    ASSERT_EQ(res.size(), 0u); // No trade happened as we didn;t have enough in Asks at given price
    EXPECT_EQ(book.Asks.size(), 2u); // sell order still there
    it = book.Asks.begin();   
    EXPECT_EQ(it->second.totalQuantity + (++it)->second.totalQuantity, 80);  // no change in Asks
    
}

// Scenario 12: Cancel a valid resting Order in book
TEST(CANCEL_ORDER, VALID_CANCEL) {
    OrderBook book;
    auto a = limit(Side::SELL, 101, 50);
    auto res = book.addOrder(a);

    auto b = limit(Side::BUY, 101, 80);
    res = book.addOrder(b);

    ASSERT_TRUE(res.size());    // 1 trade happen;

    //verify trade
    const Trade& t1 = res[0];
    EXPECT_EQ(t1.price, ticksOf(101));
    EXPECT_EQ(t1.quantity, 50);
    EXPECT_EQ(t1.buyOrderId, b.Oid);
    EXPECT_EQ(t1.sellOrderId, a.Oid);

    // verify book
    EXPECT_EQ(book.Asks.size(), 0u);  // 0 asks
    EXPECT_EQ(book.Bids.size(), 1u);  // 1 resting order;
    EXPECT_EQ(book.Bids.begin()->second.orders.front().Oid, b.Oid);   // b is only remaing order
    EXPECT_EQ(book.Bids.begin()->second.orders.front().remQuantity, 30);   // b is only remaing order

    // Now cancel B and A
    auto status = book.optimalCancelOrder(a.Oid);
    ASSERT_FALSE(status);      // Status must be false as a order is not is book

    status = book.optimalCancelOrder(b.Oid);
    ASSERT_TRUE(status);     // must be true as b is valid resting order in book

    status = book.optimalCancelOrder(b.Oid);
    ASSERT_FALSE(status);    // Double free shouldn;t be allowed, so it shoudl return false

} 