#include "../include/OrderBook.hpp"
#include <atomic>
#include <cassert>
#include <random>
#include <vector>
#include <chrono>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <iostream>
#include <cassert> // Required header

const int  SampleSize = 10000000;   // 10M orders


                         
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


class AverageBaseLine {
    private:
        // Global variables
        std::mutex mtx;
        std::condition_variable cv;
        std::queue<Order> oqueue;
        bool finished = false;
        int orderCount;

        // Push Orders in Queue
        void Producer(std::vector<Order> sample) { 
            while (!sample.empty()) {
                {
                    std::lock_guard<std::mutex>  lock(mtx);
                    oqueue.push(sample.back());
                }
                cv.notify_one();
                sample.pop_back();
            } 
            {
                std::lock_guard<std::mutex>  lock(mtx);
                finished = true;
            }
            cv.notify_one();
        }

        void Consumer() {
            while(true) {
                {
                    std::unique_lock<std::mutex> ulock(mtx);
                    cv.wait(ulock, [this] () {
                        return finished || !oqueue.empty();
                    });

                    if (finished && oqueue.empty()) {
                        break;
                    }
                    
                    oqueue.pop();
                    orderCount += 1;
                }
            }

        }

    public:
        AverageBaseLine(): orderCount(0)  {
            auto sample1 = getBenchData();
            auto start = std::chrono::steady_clock::now(); 
            {
                std::jthread producer(&AverageBaseLine::Producer, this, std::move(sample1) );
                std::jthread consumer(&AverageBaseLine::Consumer, this);
            }
            auto end = std::chrono::steady_clock::now();

            if (orderCount != SampleSize) { std::cerr << "LOST ORDERS: " << orderCount << "\n"; std::abort(); }
            
            auto elapsed_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            double nsPerOrder = static_cast<double>(elapsed_time) / SampleSize;
            std::cout << "MT Time Elapsed: " << elapsed_time << "ns , throughput :" << nsPerOrder << " ns/order" << std::endl;
        }
};
// void averageBaseLine(std::vector<Order>& sample) {
//     OrderBook book(5000000);  // As per given sample size 50L pool size shoudl handle this
//     auto start = std::chrono::steady_clock::now();
//     int MaxRestingOrder = -1;

//     for (auto& order: sample) {
//         book.addOrder(order);
//         // if (static_cast<int>(book.OrdersInBook.size()) > MaxRestingOrder ) {
//         //     MaxRestingOrder = book.OrdersInBook.size();
//         // }
//     }
//     auto end = std::chrono::steady_clock::now();
//     auto elapsed_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
//     double nsPerOrder = static_cast<double>(elapsed_time) / SampleSize;
//     std::cout << "Time Elapsed: " << elapsed_time << "ns , throughput :" << nsPerOrder << " ns/order" << std::endl;
    
// }

// void percentileBaseline(std::vector<Order>& sample) {
//     OrderBook book(5000000);    //// As per given sample size 50Lakh pool size shoudl handle this
//     std::vector<uint64_t> timeTaken;
//     timeTaken.reserve(SampleSize);

//     for (auto& order: sample) {
//         auto start = std::chrono::steady_clock::now();
//         book.addOrder(order);
//         auto end = std::chrono::steady_clock::now();
//         auto elapsed_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
//         timeTaken.push_back(elapsed_time);
//         // if (static_cast<int>(book.OrdersInBook.size()) > MaxRestingOrder ) {
//         //     MaxRestingOrder = book.OrdersInBook.size();
//         // }
//     }
    
//     sort(timeTaken.begin(), timeTaken.end());
//     std::cout << "P50: " << timeTaken[0.5 * SampleSize] << " ns |  P99: " << timeTaken[0.99 * SampleSize] << " ns |  P99.9: " << timeTaken[0.999 * SampleSize] << "ns " <<  std::endl;
// }

int main() {
    AverageBaseLine avl;
}
