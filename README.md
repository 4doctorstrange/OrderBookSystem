# Order Book — Stage 2: Single-Thread Latency Optimization (`M2`)

> This branch takes the correct `std::map`-based engine (on `main`) and rebuilds its hot data
> structures for **cache locality** and **zero hot-path allocation**. For the full engine
> overview, test suite, and the side-by-side performance journal, see the **`main`** branch.
> The concurrency work builds on this branch in **`M3`**.

---

## What this branch changes

Replace the baseline `std::map<price, PriceLevel>` + `std::list<Order>` with:

1. **Flat array of price levels** indexed by tick — `std::vector<PriceLevel>` addressed by
   `price − MIN_TICK`. O(1) level access instead of O(log P) map lookup.
2. **Occupancy bitmap** (`BitPool2000`) — one bit per level; "next non-empty level" becomes a
   hardware **bit-scan** (`std::countr_zero` / `countl_zero`) instead of a linear array walk.
3. **Object pool + intrusive linked list** — all `Order`s live in one preallocated arena
   (`OrderPool`); each `PriceLevel` is just `headIdx` / `tailIdx`, and orders are linked via
   `nextIdx` / `prevIdx` **indices** inside `Order`. No per-order `std::list` node allocation.
4. **O(1) free-list** — `acquire()` pops a free slot, `removeOrder` pushes it back — replacing an
   O(n) scan for a free slot.

Handles are **integer indices, not pointers**, so the pool is realloc-safe and the whole
`OrderBook` is trivially copyable.

---

## Benchmark journal (10M orders, `-O3 -march=native`, Apple Silicon)

Same-session medians (5 runs each). Full table + caveats live on `main`.

| Variant | avg ns/order | P-50 | P-99 | P-99.9\* |
|---|---:|---:|---:|---:|
| `std::map` baseline (`main`) | 998 | 750 | **3083** | ~24 µs |
| flat `vector<PriceLevel>` by tick | 1215 | 333 | 6708 | ~16 µs |
| flat array + occupancy bitmap | 1206 | 333 | 6750 | ~16 µs |
| **+ object pool + intrusive list** | **936** | **250** | 5042 | ~13 µs |

\* P-99.9 is dominated by OS scheduler / thermal state on an unpinned laptop — treat as noisy;
lean on avg / P-50 / P-99.

---

## Findings (the honest story)

- **Flat array trades average for median:** median halves (750 → 333 ns) via O(1) level access,
  but average **regresses** on this dense, deep-book workload (~4M peak resting orders).
- **The occupancy bitmap is neutral here** — the benchmark spans only ~201 dense ticks, so the
  "find next level" scan it removes was never the bottleneck. It wins on **sparse / wide-tick**
  books, not this one.
- **Object pool + intrusive list is the first *net* win:** removing per-order `std::list` node
  allocation pulls the average back to **936 ns — below the `std::map` baseline (998)** while
  keeping the **best median (250 ns)**. Only P-99 still favours the map.
- **A sampling profiler under-sold allocation:** `sample` showed `malloc`/`free` at only ~4% of
  self-time, yet removing `std::list` gave a ~20% average gain — the profiler's leaf self-time
  missed the allocator bookkeeping + cache-locality cost.
- **Why O(1) cancel matters:** in real HFT most orders are cancelled, not filled (cancel/trade
  ratios often >90%) as quotes chase fair value — so the `OrdersInBook` id→index map + intrusive
  unlink (O(1) cancel) is a first-class concern, not a nicety.

---

## Data structures

| Structure | Purpose |
|---|---|
| `std::vector<PriceLevel> Bids / Asks` (by tick index) | O(1) price-level access |
| `BitPool2000` occupancy bitmaps | O(1) next-non-empty level via hardware bit-scan |
| `std::vector<Order> OrderPool` + `std::vector<int> FreeList` | preallocated arena + O(1) acquire/release |
| `Order.nextIdx / prevIdx` (intrusive) | FIFO queue with no node allocation |
| `std::unordered_map<int, int> OrdersInBook` (oid → pool index) | O(1) cancel |

---

## Build, run, test

```sh
cmake -B build && cmake --build build
ctest --test-dir build     # 17 GoogleTest cases + structural invariants
./build/order_bench        # throughput + P50/P99/P99.9
```

---

## Key takeaways

- **Integer handles > pointers:** stable across reallocation, smaller, cache-friendly, and make
  the whole book copyable.
- **Measure, don't assume:** flat-array won the median but lost the average; the pool won overall.
  Every claim here is a same-session, multi-run median — not a single lucky run.
- A profiler's **self-time can under-sell allocation** cost; the A/B benchmark is the ground truth.
