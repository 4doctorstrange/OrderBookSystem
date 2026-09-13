#include "../include/OrderBook.hpp"
#include <iostream>
#include <string>
#include <cassert>
#include <thread>
#include <condition_variable>
#include <queue>
#include <mutex>
#include <vector>
#include <optional>

// ---- small printing helpers -------------------------------------------------

static std::string sideName(Side s) { return s == Side::BUY ? "BUY" : "SELL"; }

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

// Global variables
std::mutex mtx;
std::condition_variable cv;
std::queue<Order> oqueue;
bool finished = false;

int main() {
    std::cout << "========================================\n";
    std::cout << " ORDER BOOK ENGINE - DEMO\n";
    std::cout << "========================================\n";

    OrderBook book;

    auto a = limit(Side::SELL, 501.00, 50);   // rests in asks
    auto b = limit(Side::SELL, 501.00, 60);   // rests in asks
    auto c = limit(Side::BUY, 502.00, 200);   // rests in asks
    auto d = limit(Side::SELL, 501.00, 100);   // rests in asks
    auto e = limit(Side::BUY, 501.5, 10);   // rests in asks

    std::vector<Order> vec;
    vec.push_back(a);
    vec.push_back(b);
    vec.push_back(c);
    vec.push_back(d);
    vec.push_back(e);
    

    std::jthread produce([vec = std::move(vec)] () mutable {
        while (!vec.empty()) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                oqueue.push(vec.back());
            }
            std:: cout << "pushed\n";
            cv.notify_one();
            vec.pop_back();
        }
    
        {
            std::lock_guard<std::mutex> lock(mtx);
            finished = true;
        }
        cv.notify_one();
    });

    std::jthread consume( [&] () {
        while (true) {
            Order order;
            {
                std::unique_lock<std::mutex> ulock(mtx);
                cv.wait(ulock, [] () {
                    return !oqueue.empty() || finished;
                });

                if (oqueue.empty() && finished) {
                    break;
                }

                if (!oqueue.empty()) {
                    order = oqueue.front();
                    oqueue.pop();
                    std:: cout << "popped\n";
                }
            }
            cv.notify_one();
            book.addOrder(order);
        }
        
    });

    // TODO: Main cleanup   
    #if 0
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
    #endif
    return 0;
}
