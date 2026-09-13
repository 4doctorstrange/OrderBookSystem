# Order Book — Stage 3: Lock-Free Concurrency (`M3`)

> This branch decouples **order intake** from **matching** using a **lock-free SPSC ring
> buffer** feeding a **single-writer matching thread**. It builds on the single-threaded
> engine from `M2`. For the full engine overview and the single-thread performance journal,
> see the **`main`** branch.

---

## What this branch adds

- A **lock-free single-producer / single-consumer (SPSC) ring buffer**.
- A **concurrent pipeline:** `producer thread → SPSC ring → single matching thread → book.addOrder()`.
- A **microbenchmark** comparing the lock-free handoff against a `mutex + std::queue` baseline.
- A **correctness test:** the threaded book is proven **identical** to a single-threaded
  reference built from the same order stream.

All in `microBench/multiThreadBench.cpp` (CMake target `order_mt_bench`).

---

## Why decouple intake from matching

Matching must stay **single-writer** to preserve deterministic **price-time priority** — so it
runs on **one** thread with **no locks on the book**. The queue *is* the thread boundary. In a
real exchange/HFT system the producer is the **network** (socket read + protocol parse from many
clients); decoupling lets matching run uninterrupted on a pinned core while intake overlaps and
the ring absorbs bursts. (Full ingress → engine → egress reasoning is in `OrderBook_QnA.md`.)

---

## The SPSC ring — design

| Technique | Why |
|---|---|
| **Power-of-two capacity + `& (N-1)`** | index wrap is a 1-cycle mask, not a ~20–40-cycle `%` divide |
| **`alignas(64)` on `head` / `tail`** | put them on separate cache lines → no false sharing |
| **acquire / release ordering** | producer publishes the data write *before* bumping `tail`; consumer `acquire`s `tail` *before* reading the slot |
| **Cached opposite index** (`cachedHead` / `cachedTail`) | only re-read the *other* thread's atomic when the ring looks full/empty → collapses cross-core cache-line bouncing |

---

## Benchmark story (10M orders, `-O3 -march=native`, Apple Silicon)

The lesson: **lock-free is not automatically faster** — the real cost model is **cache
coherence**, not the lock.

| Version | ns/order | vs mutex (~37) |
|---|---|---|
| `mutex + std::queue` baseline | ~37 | — |
| naive SPSC (`% 10001`) | ~59 | **lost** — non-power-of-two `%` divide every op |
| + power-of-two mask (`& (N-1)`) | ~45 | **still lost** — cache-line ping-pong (re-reading the other atomic each iteration) |
| **+ cached head/tail** | **~33** | **wins** ✅ |

Handoff is measured **in isolation** (consumer pops + counts, no matching) so the ~tens-of-ns
queue cost isn't swamped by matching (~250 ns/order). **Verified race-free with ThreadSanitizer.**

> Note: `std::queue` is `deque`-backed and allocates as it grows, so part of the SPSC win is
> *no allocation*, not purely *no lock* — the bounded ring is also the realistic/correct choice.

---

## Correctness of the concurrent engine

`PipelineSPSC` runs the full pipeline into a real `OrderBook`, then asserts the threaded book is
**identical** to a single-threaded reference built from the same stream:

- `OrdersInBook.size()` — resting-order count
- per-price-level `totalQuantity`
- `bestBid()` / `bestAsk()`

Matching is deterministic given a fixed order sequence, so identical inputs ⇒ identical book.
The pipeline passes → the concurrent engine matches correctly.

---

## Build & run

```sh
cmake -B build && cmake --build build
./build/order_mt_bench     # mutex vs SPSC handoff + pipeline correctness check
```

---

## Key takeaways

- Single-writer matching ⇒ **no locks in the matching logic**; lock-free is only for the **handoff**.
- The SPSC win came from **removing cross-core atomic reads** (cached indices), not from "no lock".
- Correctness of a lock-free queue is verified with **TSan**, not "it ran".
