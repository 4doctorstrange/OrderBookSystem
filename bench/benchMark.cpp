
#include "../include/OrderBook.hpp"
#include <random>
#include <vector>
#include <chrono>
#include <iostream>

const int  SampleSize = 10000000;

std::vector<Order> getBenchData() {
    std::mt19937 rng(42);   // random number generator, 42 is seed
    std::bernoulli_distribution sideDist(0.5); // generates a random boolean value with 50% chances of each
    std::uniform_int_distribution<int> priceDist(9900, 10100); //All values have equal probability in this range
    std::uniform_int_distribution<int> quantityDist(1,100);

    std::vector<Order> orders;
    orders.reserve(SampleSize);     // reserve space to avoid data movement /copy when more space is requested
    for (int i = 0; i < SampleSize; i++) {
        Side s = sideDist(rng) ? Side::BUY :  Side::SELL;   
        double p = priceDist(rng) / (double)utils::tickMultiplier;  // 10002 / 20  = 500.1 {the actual bid/sell price}
        int qty = quantityDist(rng);
        orders.emplace_back(s, OrderType::LIMIT, p, qty, qty);
    }

    return orders;
    
}

int main() {
    auto sample = getBenchData();
    OrderBook book;
    auto start = std::chrono::steady_clock::now();

    for (auto& order: sample) {
        book.addOrder(order);
    }
    auto end = std::chrono::steady_clock::now();
    auto elapsed_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    int throughput = SampleSize / (elapsed_time/1e6);  // Number of order executed per ms
    std::cout << "Time Elapsed: " << elapsed_time << "ns , throughput :" << throughput << " orders/ms" << std::endl;

}

/* 
BASELINE NUMBERS:
harshrajput@JNPR-MAC-0TQJVJ OrderBook % ./build/order_bench
Time Elapsed: 7576845875ns , throughput :1319 orders/ms

Elapsed: 7.58 s for 10M orders
Throughput: 1319 orders/ms = ~1.32 M orders/sec
Average latency: 7.58e9 ns / 10e6 = ~758 ns/order

*/