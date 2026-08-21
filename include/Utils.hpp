
#pragma once

#include <chrono>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace utils {

    inline long long getTimestamp() {
         auto now = std::chrono::system_clock::now();

        auto duration = now.time_since_epoch();

        auto nanoSec = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
        return nanoSec.count();
    }

    const uint8_t tickMultiplier = 20 ; /* For storing decimal/paisa factor of a share price.  */

    /*
    Note: below logic is hardcoded for fixed size of 2000
    */
    class BitPool2000 {
        private:
            static constexpr std::size_t TOTAL_BITS = 2000;   // We needs 2000 bits 
            static constexpr std::size_t BLOCK_COUNT = 32;    // we need minimum 32 unisgned 64 bit numbers for 2k bits: 32 * 64 = 2048

            // Contiguous memory allocation (32 * 64 bits = 2048 bits total capacity)
            std::array<uint64_t, BLOCK_COUNT> storage{}; 
        
        public:
            // Set a bit to 1 at a specific index (0 to 1999)
            void set(size_t index) {
                auto block  = index / 64;
                auto mask = 1ULL << (index % 64);   // 1ULL is 1 unsigned long long.  index % 64 gives local bit which needs to be set 
                storage[block] |= mask;     // OR that block with mask, the local index bit is set.
            }

            // Clear a bit to 0 at a specific index (0 to 1999)
            void clear(size_t index) {
                auto block  = index / 64;
                auto mask = 1ULL << (index % 64); 
                storage[block] &= ~mask;
            }

            // Bit Scan Forward: Find the first set bit from the RIGHT (Index 0 upward)
            /*
            Bit 0 is tucked inside storage[0].Therefore, to find the first set bit from the right, 
            you must start searching at storage[0] and move up to storage[31].
            */
            int64_t find_first_from_right() const {
            for (size_t i = 0; i < BLOCK_COUNT; ++i) {
                if (storage[i] != 0) { // Skips 64 empty bits in a single CPU cycle
                    int bit_offset = std::countr_zero(storage[i]);
                    return static_cast<int64_t>((i * 64) + bit_offset);
                }
            }
            return -1; // No bits are set
            }

            // Bit Scan Reverse: Find the first set bit from the LEFT (Index 1999 downward)
            int64_t find_first_from_left() const {
                // Walk backwards through the 64-bit blocks
                for (int64_t i = static_cast<int64_t>(BLOCK_COUNT) - 1; i >= 0; --i) {
                    uint64_t chunk = storage[i];
                    
                    // Safety Step: Clear out the 48 unused padding bits in the final block (index 31)
                    // 2000 % 64 = 16 bits used. This mask keeps bits 0 to 15 active.
                    if (i == static_cast<int64_t>(BLOCK_COUNT) - 1) {
                        constexpr uint64_t safety_mask = (1ULL << (TOTAL_BITS % 64)) - 1;
                        chunk &= safety_mask;
                    }

                    if (chunk != 0) {
                        // 63 minus leading zeros calculates the exact position of the highest set bit
                        int bit_offset = 63 - std::countl_zero(chunk);
                        return (i * 64) + bit_offset;
                    }
                }
                return 2001; // No bits are set
            }
    };

}

