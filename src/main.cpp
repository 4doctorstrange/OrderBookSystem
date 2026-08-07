#include "../include/OrderBook.hpp"
#include <iostream>
#include <string>
#include <cassert>
#include <optional>

// ---- small printing helpers -------------------------------------------------

static std::string sideName(Side s) { return s == Side::BUY ? "BUY" : "SELL"; }

static void printTrades(const std::string& label, const std::vector<Trade>& trades) {
    std::cout << "\n[" << label << "] -> " << trades.size() << " trade(s)\n";
    for (const auto& t : trades) {
        std::cout << "    TRADE  buy#" << t.buyOrderId
                  << "  sell#" << t.sellOrderId
                  << "  qty " << t.quantity
                  << "  @ " << static_cast<double>(t.price) / utils::tickMultiplier << "\n";
    }
    if (trades.empty())
        std::cout << "    (no match - order rested / dropped)\n";
}

static void printBook(OrderBook& book) {
    std::cout << "  --- BOOK ---\n";
    std::cout << "  ASKS (low->high):\n";
    for (auto& [price, level] : book.Asks) {
        std::cout << "    " << static_cast<double>(price) / utils::tickMultiplier << " : ";
        for (auto& o : level.orders) std::cout << "#" << o.Oid << "(" << o.remQuantity << ") ";
        std::cout << "\n";
    }
    std::cout << "  BIDS (high->low):\n";
    for (auto& [price, level] : book.Bids) {
        std::cout << "    " << static_cast<double>(price)  / utils::tickMultiplier << " : ";
        for (auto& o : level.orders) std::cout << "#" << o.Oid << "(" << o.remQuantity << ") ";
        std::cout << "\n";
    }
    std::cout << "  ------------\n";
}

// helper: build a limit order (remQuantity starts equal to quantity)
static Order limit(Side s, double price, int qty) {
    return Order(s, OrderType::LIMIT, price, qty, qty);
}
static Order market(Side s, int qty) {
    return Order(s, OrderType::MARKET, 0.0, qty, qty);
}

// ----------------------------------------------------------------------------
// Top-of-book tests: bestBid() / bestAsk() / spread()
// Prices are stored in ticks (price * tickMultiplier), so expected values use
// this helper to convert a human price into the tick value the book returns.
static int64_t ticksOf(double price) {
    return static_cast<int64_t>(price * utils::tickMultiplier);
}

template <typename Opt>
static std::string optStr(const Opt& o) {
    return o ? std::to_string(*o) : std::string("<none>");
}

static void testTopOfBook() {
    std::cout << "\n### TEST: bestBid / bestAsk / spread ###\n";
    OrderBook book;

    // 1) Empty book -> all three are empty (nullopt).
    assert(!book.bestBid().has_value()  && "empty book: bestBid must be nullopt");
    assert(!book.bestAsk().has_value()  && "empty book: bestAsk must be nullopt");
    assert(!book.spread().has_value()   && "empty book: spread must be nullopt");

    // 2) One resting bid @100 -> bestBid set, ask/spread still empty.
    auto b1 = limit(Side::BUY, 100.00, 10);
    book.addOrder(b1);
    assert(book.bestBid().has_value() && *book.bestBid() == ticksOf(100.00));
    assert(!book.bestAsk().has_value());
    assert(!book.spread().has_value() && "spread needs both sides");

    // 3) One resting ask @101 -> bestAsk set, spread = ask - bid = 1.00 in ticks.
    auto a1 = limit(Side::SELL, 101.00, 10);
    book.addOrder(a1);
    assert(*book.bestAsk() == ticksOf(101.00));
    assert(book.spread().has_value());
    assert(*book.spread() == ticksOf(101.00) - ticksOf(100.00) && "spread = ask - bid, must be positive");

    // 4) A better (higher) bid @100.50 becomes the new best bid; spread tightens.
    auto b2 = limit(Side::BUY, 100.50, 5);
    book.addOrder(b2);
    assert(*book.bestBid() == ticksOf(100.50) && "best bid = highest bid");
    assert(*book.spread() == ticksOf(101.00) - ticksOf(100.50));

    // 5) A better (lower) ask @100.75 becomes the new best ask; spread tightens again.
    auto a2 = limit(Side::SELL, 100.75, 5);
    book.addOrder(a2);
    assert(*book.bestAsk() == ticksOf(100.75) && "best ask = lowest ask");
    assert(*book.spread() == ticksOf(100.75) - ticksOf(100.50));

    // 6) Consume the entire best-bid level (100.50) with a crossing sell.
    //    best bid should fall back to the next level (100.00).
    auto s = limit(Side::SELL, 100.50, 5);   // fills b2 (5 @ 100.50) exactly
    book.addOrder(s);
    assert(*book.bestBid() == ticksOf(100.00) && "after consuming top level, best bid drops to next");

    // 7) Cancel remaining bid -> bid side empty -> bestBid & spread empty again.
    book.optimalCancelOrder(b1.Oid);
    assert(!book.bestBid().has_value() && "no bids left -> bestBid nullopt");
    assert(!book.spread().has_value());

    std::cout << "All top-of-book assertions passed.\n";
}

// ----------------------------------------------------------------------------

int main() {
    std::cout << "========================================\n";
    std::cout << " ORDER BOOK ENGINE - DEMO\n";
    std::cout << "========================================\n";

    // Scenario 1: two limits rest (no cross), then a buy that fully fills one.
    {
        std::cout << "\n### Scenario 1: rest then full fill ###\n";
        OrderBook book;

        auto a = limit(Side::SELL, 101.00, 50);   // rests in asks
        printTrades("SELL 50 @ 101", book.addOrder(a));

        auto b = limit(Side::BUY, 100.00, 100);    // rests in bids (no cross)
        printTrades("BUY 100 @ 100", book.addOrder(b));
        printBook(book);

        auto c = limit(Side::BUY, 101.00, 50);     // crosses, fully fills the ask
        printTrades("BUY 50 @ 101", book.addOrder(c));
        printBook(book);
    }

    // Scenario 2: sweep multiple ask levels (price priority).
    {
        std::cout << "\n### Scenario 2: sweep multiple levels ###\n";
        OrderBook book;

        auto a = limit(Side::SELL, 101.70, 50);
        auto b = limit(Side::SELL, 101.05, 50);
        book.addOrder(a);
        book.addOrder(b);
        printBook(book);

        auto c = limit(Side::BUY, 102.00, 70);     // takes 50@101 then 20@102
        printTrades("BUY 70 @ 102", book.addOrder(c));
        printBook(book);
    }

    // Scenario 3: FIFO / time priority at the same price.
    {
        std::cout << "\n### Scenario 3: time priority (FIFO) ###\n";
        OrderBook book;

        auto a = limit(Side::SELL, 101.00, 50);    // arrives first
        auto b = limit(Side::SELL, 101.00, 30);    // arrives later
        book.addOrder(a);
        book.addOrder(b);
        printBook(book);

        auto c = limit(Side::BUY, 101.00, 60);     // fills #a fully (50), then 10 of #b
        printTrades("BUY 60 @ 101", book.addOrder(c));
        printBook(book);
    }

    // Scenario 4: partial fill, remainder rests.
    {
        std::cout << "\n### Scenario 4: partial fill, remainder rests ###\n";
        OrderBook book;

        auto a = limit(Side::SELL, 101.00, 30);
        book.addOrder(a);

        auto b = limit(Side::BUY, 101.00, 50);     // fills 30, 20 rests as a bid
        printTrades("BUY 50 @ 101", book.addOrder(b));
        printBook(book);
    }

    // Scenario 5: market order - fills best, leftover dropped (not rested).
    {
        std::cout << "\n### Scenario 5: market order ###\n";
        OrderBook book;

        auto a = limit(Side::SELL, 101.00, 50);
        book.addOrder(a);

        auto b = market(Side::BUY, 80);            // fills 50, leftover 30 dropped
        printTrades("BUY 80 MARKET", book.addOrder(b));
        printBook(book);
    }

    // Scenario 6: cancel a resting order, then confirm matching skips it.
    {
        std::cout << "\n### Scenario 6: cancel a resting order ###\n";
        OrderBook book;

        auto a = limit(Side::SELL, 101.00, 50);
        auto b = limit(Side::SELL, 101.00, 30);    // same level, behind a (FIFO)
        auto c = limit(Side::SELL, 102.00, 40);
        book.addOrder(a);
        book.addOrder(b);
        book.addOrder(c);
        std::cout << "Resting order ids -> a=#" << a.Oid
                  << " b=#" << b.Oid << " c=#" << c.Oid << "\n";
        printBook(book);

        std::cout << "\n-> cancel #" << a.Oid << " (front of 101 level)\n";
        book.optimalCancelOrder(a.Oid);
        printBook(book);   // expect: 101 -> only b, 102 -> c

        std::cout << "\n-> cancel #" << c.Oid << " (only order at 102, level should vanish)\n";
        book.optimalCancelOrder(c.Oid);
        printBook(book);   // expect: 102 level gone entirely

        // Now a BUY should match the remaining b (#b) at 101, not the cancelled a.
        auto d = limit(Side::BUY, 101.00, 30);
        printTrades("BUY 30 @ 101 (should hit #" + std::to_string(b.Oid) + ")",
                    book.addOrder(d));
        printBook(book);
    }

    // Scenario 7: IOC order — partial fill, remainder dropped (NOT rested)
    {
        std::cout << "\n### Scenario 7: IOC (Immediate-Or-Cancel) ###\n";
        OrderBook book;

        auto a = limit(Side::SELL, 101, 50);
        auto b = limit(Side::SELL, 102, 30);
        book.addOrder(a);
        book.addOrder(b);
        printBook(book);

        // IOC BUY 70 @ 101: only 50 available at <=101, fills 50, remaining 20 DROPPED
        auto c = Order(Side::BUY, OrderType::IOC, 101, 70, 70);
        printTrades("IOC BUY 70 @ 101 (should fill 50, drop 20)", book.addOrder(c));
        printBook(book);  // expect: 102 ask still there, NO bid resting
    }

    // Scenario 8: IOC fully filled
    {
        std::cout << "\n### Scenario 8: IOC fully filled ###\n";
        OrderBook book;

        auto a = limit(Side::SELL, 101, 50);
        book.addOrder(a);

        // IOC BUY 30 @ 101: 50 available, only need 30 -> fully filled
        auto c = Order(Side::BUY, OrderType::IOC, 101, 30, 30);
        printTrades("IOC BUY 30 @ 101 (fully filled)", book.addOrder(c));
        printBook(book);  // expect: ask has 20 remaining
    }

    // Scenario 9: FOK rejected — not enough quantity
    {
        std::cout << "\n### Scenario 9: FOK rejected (insufficient qty) ###\n";
        OrderBook book;

        auto a = limit(Side::SELL, 101, 50);
        book.addOrder(a);
        printBook(book);

        // FOK BUY 80 @ 101: need 80 but only 50 available -> REJECT entirely
        auto c = Order(Side::BUY, OrderType::FOK, 101, 80, 80);
        printTrades("FOK BUY 80 @ 101 (REJECT, only 50 avail)", book.addOrder(c));
        printBook(book);  // expect: book UNCHANGED (ask 50 still there)
    }

    // Scenario 10: FOK accepted — exact quantity available
    {
        std::cout << "\n### Scenario 10: FOK accepted (enough qty) ###\n";
        OrderBook book;

        auto a = limit(Side::SELL, 101, 50);
        auto b = limit(Side::SELL, 101, 30);
        book.addOrder(a);
        book.addOrder(b);
        printBook(book);

        // FOK BUY 80 @ 101: need 80, available = 50+30 = 80 -> FILL ALL
        auto c = Order(Side::BUY, OrderType::FOK, 101, 80, 80);
        printTrades("FOK BUY 80 @ 101 (should fill all 80)", book.addOrder(c));
        printBook(book);  // expect: book empty
    }

    // Scenario 11: FOK rejected — price limit exceeded
    {
        std::cout << "\n### Scenario 11: FOK rejected (price limit) ###\n";
        OrderBook book;

        auto a = limit(Side::SELL, 101, 50);
        auto b = limit(Side::SELL, 103, 50);
        book.addOrder(a);
        book.addOrder(b);
        printBook(book);

        // FOK BUY 80 @ 102: need 80, only 50 at <=102 -> REJECT
        auto c = Order(Side::BUY, OrderType::FOK, 102, 80, 80);
        printTrades("FOK BUY 80 @ 102 (only 50 avail at <=102, REJECT)", book.addOrder(c));
        printBook(book);  // expect: book UNCHANGED
    }

    testTopOfBook();

    std::cout << "\nDone.\n";
    return 0;
}
