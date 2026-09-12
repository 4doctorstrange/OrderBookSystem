#include "../include/OrderBook.hpp"
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <random>
#include <vector>
#include <chrono>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <iostream>
#include <cassert> // Required header
#include <atomic>

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

            if (orderCount != SampleSize) { std::cerr << "LOST ORDERS MUTEX: " << orderCount << "\n"; std::abort(); }
            
            auto elapsed_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            double nsPerOrder = static_cast<double>(elapsed_time) / SampleSize;
            std::cout << "MT Time Elapsed: " << elapsed_time << "ns , throughput :" << nsPerOrder << " ns/order" << std::endl;
        }
};


class SPSC {
    private:
        alignas(64) std::atomic<int> head{0}; // pop
        alignas(64) std::atomic<int> tail{0}; // push
        int Capacity;
        std::vector<Order> oqueue;
        int orderCount;
    
        void Producer(std::vector<Order> sample) {
            int cachedHead = 0; 
            for (int i = 0; i < SampleSize; i++) {
                
                auto tail_idx = tail.load(std::memory_order_relaxed);
                auto next = (tail_idx + 1) & (Capacity - 1);
                
                
                while (next == cachedHead) {
                    cachedHead = head.load(std::memory_order_acquire);
                }
                
                oqueue[tail_idx] = sample.back();
                sample.pop_back();
                tail.store(next, std::memory_order_release);
                
                
            }
        }

        void Consumer() {
            int cachedTail = 0;   
            for (int i = 0; i < SampleSize; i++) {
                auto head_idx = head.load(std::memory_order_relaxed);
                // get fresh copy of tail when colllision 
                while (head_idx == cachedTail) {
                    cachedTail = tail.load(std::memory_order_acquire);
                }
                // oqueue[head_idx] = Order(); // Make it a null or fresh order;
                orderCount += 1;
                head.store( (head_idx + 1) & (Capacity - 1), std::memory_order_release);
            }
        }

    public: 
        SPSC(): orderCount(0), Capacity(1 << 14)  {
            // lets set capacity of  oqueue to be 10000
            oqueue.resize(Capacity);

            auto sample1 = getBenchData();
            
            auto start  = std::chrono::steady_clock::now();
            {
                std::jthread producer(&SPSC::Producer, this, std::move(sample1));
                std::jthread consumer(&SPSC::Consumer, this);
            }
            auto end = std::chrono::steady_clock::now();

            if (orderCount != SampleSize) {
                std::cerr << "LOST ORDERS in SPSC: " << orderCount << "\n";
                std::abort();
            }

            auto elapsed_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            double nsPerOrder = static_cast<double>(elapsed_time) / SampleSize;
            std::cout << "SPSC Time Elapsed: " << elapsed_time << "ns , throughput :" << nsPerOrder << " ns/order" << std::endl;
        }


};


int main() {
    AverageBaseLine avl;
    SPSC sp;
}
