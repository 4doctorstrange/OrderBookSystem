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


// 1 threaded 
class AverageBaseLine {
    private:
        std::vector<Order> sample;
        OrderBook book;
    public:
        AverageBaseLine(std::vector<Order> orders): sample(std::move(orders)) {
            book = OrderBook(5000000);
            for (auto& order: sample) {
                book.addOrder(order);
            }
        }
        
        OrderBook getAverageBaseLineBook() {
            return book;
        }

};

//  mutex based Producer-Consumer
class AverageBaseLinePC {
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
        AverageBaseLinePC(): orderCount(0)  {
            auto sample1 = getBenchData();
            auto start = std::chrono::steady_clock::now(); 
            {
                std::jthread producer(&AverageBaseLinePC::Producer, this, std::move(sample1) );
                std::jthread consumer(&AverageBaseLinePC::Consumer, this);
            }
            auto end = std::chrono::steady_clock::now();

            if (orderCount != SampleSize) { std::cerr << "LOST ORDERS MUTEX: " << orderCount << "\n"; std::abort(); }
            
            auto elapsed_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            double nsPerOrder = static_cast<double>(elapsed_time) / SampleSize;
            std::cout << "MT Time Elapsed: " << elapsed_time << "ns , throughput :" << nsPerOrder << " ns/order" << std::endl;
        }
};


// lock free  Single Producer-Consumer
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
                
                oqueue[tail_idx] = sample[i];
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


class PipelineSPSC {
    private:
        alignas(64) std::atomic<int> head{0}; // pop
        alignas(64) std::atomic<int> tail{0}; // push
        int Capacity;
        std::vector<Order> oqueue;
        int orderCount;
        OrderBook book;
        std::vector<Order> sample;
    
        void Producer() {
            int cachedHead = 0; 
            for (int i = 0; i < SampleSize; i++) {
                
                auto tail_idx = tail.load(std::memory_order_relaxed);
                auto next = (tail_idx + 1) & (Capacity - 1);
                
                
                while (next == cachedHead) {
                    cachedHead = head.load(std::memory_order_acquire);
                }
                
                oqueue[tail_idx] = sample[i];
                // sample.pop_back();
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
                orderCount += 1;
                book.addOrder(oqueue[head_idx]);     // Add order in book;
                head.store( (head_idx + 1) & (Capacity - 1), std::memory_order_release);
            }
        }

    public: 
        PipelineSPSC(std::vector<Order> sample): orderCount(0), Capacity(1 << 14) , sample(std::move(sample)) {
            // lets set capacity of  oqueue to be 10000
            oqueue.resize(Capacity);
            book = OrderBook(5000000);
            {
                std::jthread producer(&PipelineSPSC::Producer, this);
                std::jthread consumer(&PipelineSPSC::Consumer, this);
            }
            auto end = std::chrono::steady_clock::now();

            if (orderCount != SampleSize) {
                std::cerr << "LOST ORDERS in Pipelined SPSC: " << orderCount << "\n";
                std::abort();
            }
        }

        OrderBook getPipeLineBook() {
            return book;
        }
};

int main() {
    // AverageBaseLinePC avl;
    // SPSC sp;

    auto orderList = getBenchData();
    AverageBaseLine avg(orderList);
    PipelineSPSC pipeline(orderList);

    auto avgBaseLineBook = avg.getAverageBaseLineBook();
    auto pipeLineBook = pipeline.getPipeLineBook();

    // ALL THE BELOW ASSERTS MUST PASS:

    // check resting orders in book
    assert(avgBaseLineBook.OrdersInBook.size() == pipeLineBook.OrdersInBook.size());

    //Check BIDS:
    assert(avgBaseLineBook.Bids.size() == pipeLineBook.Bids.size());
    assert(avgBaseLineBook.bestBid() == pipeLineBook.bestBid());

    auto BidsSize = avgBaseLineBook.Bids.size();
    for (int i = 0; i < BidsSize; i++) {
        auto avgPriceLevel = avgBaseLineBook.Bids[i];
        auto pipePriceLevel = pipeLineBook.Bids[i];

        assert(avgPriceLevel.headIdx == pipePriceLevel.headIdx);
        assert(avgPriceLevel.totalQuantity == pipePriceLevel.totalQuantity);
    }


    // Check Asks
    assert(avgBaseLineBook.Asks.size() == pipeLineBook.Asks.size());
    assert(avgBaseLineBook.bestAsk() == pipeLineBook.bestAsk());


    auto AsksSize = avgBaseLineBook.Asks.size();
    for (int i = 0; i < AsksSize; i++) {
        auto avgPriceLevel = avgBaseLineBook.Asks[i];
        auto pipePriceLevel = pipeLineBook.Asks[i];
        assert(avgPriceLevel.headIdx == pipePriceLevel.headIdx);
        assert(avgPriceLevel.totalQuantity == pipePriceLevel.totalQuantity);
    }

}
