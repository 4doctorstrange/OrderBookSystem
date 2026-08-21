
#include "../include/OrderBook.hpp"
#include <atomic>
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


void averageBaseLine(std::vector<Order>& sample) {
    OrderBook book;
    auto start = std::chrono::steady_clock::now();

    for (auto& order: sample) {
        book.addOrder(order);
    }
    auto end = std::chrono::steady_clock::now();
    auto elapsed_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double nsPerOrder = static_cast<double>(elapsed_time) / SampleSize;
    std::cout << "Time Elapsed: " << elapsed_time << "ns , throughput :" << nsPerOrder << " ns/order" << std::endl;
}

void percentileBaseline(std::vector<Order>& sample) {
    OrderBook book;
    std::vector<uint64_t> timeTaken;
    timeTaken.reserve(SampleSize);

    for (auto& order: sample) {
        auto start = std::chrono::steady_clock::now();
        book.addOrder(order);
        auto end = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        timeTaken.push_back(elapsed_time);
    }
    
    sort(timeTaken.begin(), timeTaken.end());
    std::cout << "P50: " << timeTaken[0.5 * SampleSize] << " ns |  P99: " << timeTaken[0.99 * SampleSize] << " ns |  P99.9: " << timeTaken[0.999 * SampleSize] << "ns " <<  std::endl;
}

int main() {
    auto sample1 = getBenchData();
    averageBaseLine(sample1);

    auto sample2 = getBenchData();
    percentileBaseline(sample2);

}

/* 
BASELINE NUMBERS (./build/order_bench):
Time Elapsed: 8338592542ns , throughput :833.859 ns/order

P50: 708 ns |  P99: 2625 ns |  P99.9: 5250ns 
P50 = 708 ns → "50% of orders took ≤ 708 ns."
P99 = 2625 ns → "slowest 1% took more than 2625 ns"
P99.9 = 5250 ns → "only the slowest 0.1% took more than 5250 ns."

*/