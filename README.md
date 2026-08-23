# Order Book Matching Engine

A limit order book (LOB) matching engine written in modern C++ (C++20) — the core
component of every exchange. Supports multiple order types, price-time (FIFO) priority
matching, and O(1) order cancellation, with a GoogleTest suite verifying exact trade
output and structural invariants.

> Built as a hands-on study of low-latency systems design and the data-structure /
> memory trade-offs that matter for high-frequency trading (HFT).

---

## Features

- **Order types:** Limit, Market, IOC (Immediate-Or-Cancel), FOK (Fill-Or-Kill)
- **Price-time priority (FIFO):** best price first; ties broken by arrival order
- **O(1) cancellation** via an order-id → position index
- **Integer tick-based pricing** — prices stored as `int64_t` ticks to avoid
  floating-point rounding error
- **Top-of-book queries:** best bid, best ask, spread (all O(1))
- **Tested:** 13 GoogleTest cases asserting exact trade output *and* resulting book
  state, plus a structural invariant checker run throughout the scenarios

---

## Design & Architecture

| Class        | Responsibility                                                        |
|--------------|-----------------------------------------------------------------------|
| `Order`      | Order data: id, side, type, price (ticks), quantity, remaining qty     |
| `Trade`      | An execution: buy id, sell id, price, quantity                         |
| `PriceLevel` | One price bucket: a FIFO list of orders + aggregate quantity           |
| `OrderBook`  | The engine: bids/asks, the id index, and `addOrder` / `match` / cancel |

### Data structures

| Structure                                             | Purpose                              |
|-------------------------------------------------------|--------------------------------------|
| `std::map<int64_t, PriceLevel, std::greater<>>` (bids)| price levels sorted high→low         |
| `std::map<int64_t, PriceLevel>` (asks)                | price levels sorted low→high         |
| `std::list<Order>` (inside each level)                | FIFO queue (stable iterators)        |
| `std::unordered_map<int, std::list<Order>::iterator>` | order-id → position, for O(1) cancel |

### Complexity

| Operation        | Complexity                          |
|------------------|-------------------------------------|
| Add limit order  | O(log P) to find level + O(1) append|
| Match / fill     | O(1) to read best + O(k) for k fills|
| Cancel order     | O(1) via the id index               |
| Best bid / ask   | O(1)                                |

Where `P` = number of distinct price levels.

### The core rule — price-time priority

```
while (incoming order has quantity) AND (best opposite level crosses its price):
    take the front (oldest) order at that level
    fill min(incoming, resting); emit a Trade at the resting order's price
    remove any order/level that reaches zero
leftover -> rest in book (Limit) or drop (Market / IOC), FOK is all-or-nothing
```

---

## Build & Run

Requires CMake ≥ 3.16, a C++20 compiler, and GoogleTest (`brew install googletest`).

```sh
cmake -B build
cmake --build build

./build/myapp            # interactive demo (matching scenarios)
ctest --test-dir build   # run the test suite
./build/order_bench      # run the throughput benchmark
```

---

## Testing

- **Framework:** GoogleTest + CTest
- **Coverage:** full/partial fills, multi-level sweeps, FIFO time priority, market
  orders, IOC/FOK semantics, cancellation (front, empty-level removal, filled, and
  double-cancel), and top-of-book queries.
- **Invariants:** a `checkInVariants()` helper verifies structural consistency —
  no empty levels, positive resting quantities, price/key agreement, per-level
  `totalQuantity` sums, and that the id index exactly mirrors the resting orders.

```sh
ctest --test-dir build --output-on-failure
```

---

## Benchmark & performance journal

Single-threaded, `-O3 -march=native`, 10,000,000 randomized `LIMIT` orders (seed 42)
with prices uniform over 201 ticks (a heavy add + match workload; ~4M peak resting
orders), Apple Silicon laptop. Latency measured per order with `steady_clock`. All
variants re-run **5× back-to-back in one sitting**; values below are the **medians**.

| Variant (branch / commit)              | avg ns/order | P-50   | P-99     | P-99.9\*  |
|----------------------------------------|-------------:|-------:|---------:|----------:|
| `std::map` price levels — baseline (`main`)          | 998  | 750 ns | **3083 ns** | ~24 µs |
| flat `vector<PriceLevel>` by tick (`M2` @ `3300d30`) | 1215 | 333 ns | 6708 ns | ~16 µs |
| flat array + occupancy bitmap (`M2` @ `cef560a`)     | 1206 | 333 ns | 6750 ns | ~16 µs |
| **+ object pool + intrusive list (`M2` @ `2e49e01`)** | **936** | **250 ns** | 5042 ns | ~13 µs |
| ↳ same build, single **unloaded** run (not the hot batch) | 929 | 250 ns | 5000 ns | **7167 ns** |

\* P-99.9 is **not authoritative** — on an unpinned laptop it is dominated by OS
scheduler / thermal state (this back-to-back batch ran hot; the baseline alone swung
8.7 µs → 26 µs with no code change). The pool build's **single unloaded run** (last row,
peak resting ≈ 4.05M orders) shows P-99.9 ≈ **7.2 µs** when the machine isn't thermally
throttled — so most of the ~13 µs median is machine state, not the engine. Lean on
**avg / P-50 / P-99**.

### Takeaways

- **Flat array** trades average for median: median halves (750 → 333 ns) via O(1) level
  access, but average **regresses** (998 → 1215 ns/order) on this dense, deep-book workload.
- **Occupancy bitmap** is within noise of the plain flat array here — its win is on
  *sparse / wide-tick* books, not this dense one.
- **Object pool + intrusive linked list** is the first *net* win: removing per-order
  `std::list` node allocation brings the average back to **936 ns (below the `map`
  baseline)** while keeping the **best median (250 ns)**. Only **P-99 still favours the
  map** (3083 vs 5042 ns) — an honest remaining gap.
- A sampling profile had shown `malloc`/`free` at only ~4% of self-time, which *under*-sold
  the cost: removing `std::list` also cut allocator bookkeeping and improved cache
  locality, for a ~20% average gain over the flat-array/bitmap builds.

> The optimization code lives on branch `M2`; `main` keeps the `std::map` baseline as
> the reference implementation.

---

## Roadmap

Planned work to turn a correct engine into a low-latency one:

1. **Latency percentiles** — P50 / P99 / P99.9 per operation.
2. **Single-thread optimization** — object pool / arena allocation, flat array-based
   price levels, and an intrusive linked list to eliminate hot-path allocation and
   improve cache locality.
3. **Concurrency** — single-writer matching thread fed by an SPSC lock-free ring buffer.
4. **Rigor** — sanitizers (ASan/UBSan/TSan) in CI, fuzz testing.

---

## Project structure

```
include/    Order, Trade, PriceLevel, OrderBook, Enums, Utils (headers)
src/        implementations + main.cpp (interactive demo)
test/       unitTest.cpp (GoogleTest suite + invariants)
bench/      benchMark.cpp (throughput benchmark)
CMakeLists.txt
```
