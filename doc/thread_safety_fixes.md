# Thread-safety bugs found and fixed

This document explains a set of bugs found during a deep inspection of the headers in
`include/density`, and the fixes applied for them. The first three are concurrency bugs
in the lock-free machinery; the others are ordinary C++ bugs that were found along the
way. Each one was reproduced with a dedicated stress program or sanitizer run before
being fixed, and verified again after the fix.

To follow the concurrency bugs you only need the mental model in the next section.

---

## Background: how the concurrent queues work

All the queues in density store their elements in fixed-size, fixed-alignment **memory
pages** (64 KiB by default) obtained from a shared page allocator. Inside a page,
elements are laid out linearly, each one preceded by a small **control block** whose
only field, `m_next`, points to the next control block. Since control blocks are
aligned to at least 64 bytes, the low bits of `m_next` are always zero, and the queue
uses them as **flags**:

| flag | meaning |
|---|---|
| `LfQueue_Busy` (1) | a producer or consumer is currently working on this element |
| `LfQueue_Dead` (2) | the element is gone (consumed, canceled, or padding); skip it |
| `LfQueue_External` (4) | the element lives in a separate heap block |
| `LfQueue_InvalidNextPage` (8) | this is the last block of a page and no next page is linked yet |

A value of **zero** in `m_next` means "end of the queue": the block after the last
allocated one. Producers advance an atomic **tail** pointer to claim space; consumers
advance an atomic **head** pointer over dead blocks, and *consume* an element by
compare-and-swapping the `Busy` flag into its `m_next`.

Two more mechanisms matter here:

- **Zeroed pages.** The multiple-producer tails need newly linked pages to be entirely
  zero, because "m_next == 0" is what stops both consumers and helper producers from
  running into uninitialized memory. The page allocator therefore keeps **two pools of
  free pages**: one of *zeroed* pages and one of *non-zeroed* pages, and the queue asks
  for the kind it needs (`allocate_page_zeroed()` vs `allocate_page()`).

- **Pinning.** A consumer may hold a pointer into a page that another consumer is about
  to deallocate. To make this safe, every page ends with a footer containing an atomic
  **pin count**. A thread *pins* a page (increments the count) before reading it, and
  the allocator refuses to *recycle* a page whose pin count is not zero. Deallocating a
  pinned page is allowed — the page just sits in the free-page stack until the last pin
  goes away. After pinning, a thread must *re-validate* that the page is still the one
  it thinks it is (for example by re-reading the head pointer), because the pin may
  have arrived "too late", after the page was already recycled.

With that, the bugs.

---

## Bug 1 — the default `lf_heter_queue` leaked every page it retired

**Severity: high. Affected: `lf_heter_queue` with multiple producers and sequential
consistency — which is the default configuration.**

### The problem

The sequentially-consistent multiple-producer tail allocates its pages **only from the
zeroed pool** (`create_page()` calls `allocate_page_zeroed()`), because its "help"
protocol relies on virgin blocks reading zero.

But the consume side had `s_deallocate_zeroed_pages = false`, which told the head
layer two things:

1. do **not** zero blocks while consuming them, and
2. when a page is fully consumed, return it with `deallocate_page()` — i.e. into the
   **non-zeroed pool**.

The page allocator never moves pages between the two pools and never re-zeroes a
non-zeroed page to satisfy a zeroed request. So the pages flowed one way:

```
 zeroed pool ──▶ queue ──▶ (consumed, now dirty) ──▶ non-zeroed pool ──▶ nowhere
      ▲
      └── every new page came from fresh memory requested from the system
```

A program that uses only this queue (the default lock-free queue!) never reuses a
single page. Every ~1000 small elements pushed and consumed costs one 64 KiB page,
forever.

### How it showed up

A single-threaded loop doing 4 million `push(int)` + consume cycles grew the process
from 3.7 MB to **254 MB** (about 64 bytes leaked per element). A 10-second
multithreaded stress reached 1.5 GB. The relaxed-consistency variant, the
single-producer variants and `sp_heter_queue` were all flat — only the default
configuration leaked, because it is the only one with this pool mismatch.

### The fix

`s_deallocate_zeroed_pages` is now `true` for this tail
(`lf_queue_tail_multiple_seq_cst.h`). The head zeroes each block as it consumes it and
returns pages with `deallocate_page_zeroed()`, so they go back into the pool the tail
allocates from. This is exactly the protocol the relaxed-consistency tail already used,
so no new code path was invented. The cost is a small `memset` per consumed element —
the price the relaxed queue was already paying — and the measured growth for the same
4M-element loop dropped from +250 MB to +324 kB (one-time warm-up).

---

## Bug 2 — consumers could see an "empty" queue while committed elements were present

**Severity: high for correctness. Affected: every queue with multiple consumers —
`lf_heter_queue` (both consistency models) and `sp_heter_queue`.**

### The problem

This is a classic *check-then-act* race, hidden at the very start of the consumer
iteration (`lf_queue_head_multiple.h`, `Consume::begin_iteration`). The code did,
in order:

```cpp
head = m_head.load();         // (1) where does the queue begin?
pin head's page;              // (2) make sure the page can't be recycled
head = m_head.load();         // (3) re-validate: still the same page? good.
m_next = head->m_next.load(); // (4) read the first control block
if (m_next == 0) → "the queue is empty"
```

Step (3) is the standard pin-and-revalidate dance and it is correct — but there is a
gap **between (3) and (4)**. Recall what another consumer does when it advances the
head past a consumed (dead) block B:

```cpp
m_head.compare_exchange(B, next);   // (a) move the head forward…
B->m_next.store(0);                 // (b) …then zero the old block
```

Now interleave the two threads:

| our consumer | another consumer |
|---|---|
| (3) re-reads `m_head` → still B ✓ | |
| | (a) advances `m_head` from B to `next` |
| | (b) zeroes `B->m_next` |
| (4) reads `B->m_next` → **0** | |
| concludes: *queue is empty* | |

But the queue is not empty — the elements are right behind the new head. The zero we
read does not mean "end of queue"; it means "this block was consumed a microsecond
ago". The same hole existed for in-page steps in `move_next` (the page-crossing step
already had a re-check; the in-page step did not).

Note that no amount of `seq_cst` fixes this: it is not a memory-ordering bug, it is a
time-of-check/time-of-use bug. Both reads are individually correct and current; the
state simply changed between them.

### How it showed up

An observer thread hammering `empty()` while producers kept ≥5000 committed elements
in the queue (so any "empty" answer is provably wrong) measured:

- sequentially-consistent queue: ~150 false "empty" answers per 20 s run — a direct
  violation of the documented total ordering;
- relaxed queue: **~1% of all calls** answered "empty" wrongly (there, every consumed
  block is zeroed, so the window exists at every block, not only at page boundaries);
- `sp_heter_queue`: same race, same head code.

`try_consume()` uses the same iteration, so the common shutdown idiom *"drain until
try_consume fails"* could exit while work was still queued.

### The fix

The key insight: **a block can only be zeroed after `m_head` has been advanced past
it** — that is what the (a)-then-(b) order above guarantees. So a zero is trustworthy
exactly when the head has *not* moved:

- `begin_iteration` now re-reads `m_head` after reading a zero. The pinned page cannot
  be recycled under us, so if `m_head` still equals the block we read, the zero is a
  genuine end-of-queue; if `m_head` moved, we restart from the new head.
- `move_next` records the head value the iteration started from (`m_begin_head`).
  Every block an iteration visits is at or after that position, so if any of them got
  zeroed meanwhile, `m_head` must have advanced past `m_begin_head`. On reading a
  zero, the consumer restarts **iff `m_head != m_begin_head`**. This both detects
  every concurrent zeroing and guarantees termination: when the head has not moved,
  nothing we saw can have been zeroed, the zero is genuine, and we stop — no retry
  loop is possible without real global progress.

The extra cost is one comparison on the path that was about to report "empty" (a cold
path), nothing on the hot path. After the fix the same observer ran 111 million
certified checks across all three queue types with **zero** wrong answers, and a
sum/count invariant test (3 producers, 4 consumers, 3M elements) confirmed every
element is still consumed exactly once.

---

## Bug 3 — the deferred-unpin list corrupted the free-page stack

**Severity: high (memory corruption). Affected: the public
`basic_default_allocator::unpin_page(progress_guarantee, …)` API; the queues
themselves never reached this path.**

### The problem

This one is about **intrusive linked lists sharing a link field**.

Every page footer has a single pointer, `m_next_page`, used to link the page into a
free-page stack when it is deallocated. Remember that deallocating a *pinned* page is
legal: the page then sits in the free stack, linked through `m_next_page`, until the
pin count drops to zero.

The wait-free flavor of `unpin_page` tried to decrement the pin count with a single
`compare_exchange`. Since a CAS can fail (contention, or spuriously), and "wait-free"
forbids retrying in a loop, the code deferred the decrement: it pushed the footer onto
a **thread-local "pages to unpin later" list** — which was *also* linked through
`m_next_page`:

```cpp
if (!pin_count.compare_exchange_weak(v, v - 1))
    m_pages_to_unpin.push(footer);     // writes footer->m_next_page !
```

One pointer field, two owners. If the page was in a free stack at that moment (the
caller held a pin, so the page may well have been deallocated), that `push` overwrote
the free-stack link:

- the free stack gets truncated or spliced into a thread-local list → pages silently
  leak, foreign nodes appear in the shared stack;
- the thread's later "process pending unpins" walk follows `m_next_page` into the free
  stack and decrements pin counts of pages it never pinned → the unsigned count wraps
  from 0 to 2⁶⁴−1 and that page can never be recycled again;
- two threads can even defer an unpin of the *same* page, linking one footer into two
  lists at once.

ThreadSanitizer confirmed all of this on a small program that uses only the public,
documented API (`try_pin_page`/`unpin_page` with `progress_wait_free` plus
`deallocate_page`): races between the pending-list push, the free-stack push, and
another thread's pending-list walk, all on the same footer word.

### The fix

The deferral machinery is deleted. `unpin_page` now always performs a single
`fetch_sub(1)`:

- a `fetch_sub` **cannot fail**, so there is nothing to defer — unlike a pin, an unpin
  can never be canceled or postponed, it must happen;
- on x86 (`lock xadd`) it is genuinely wait-free; on LL/SC architectures it is
  lock-free — which is exactly the guarantee the library already documents for page
  pinning (see the note in `lf_heter_queue`: wait-free operations bail out *before*
  pinning precisely because pinning is lock-free, not wait-free).

As a bonus, every pin/unpin/allocate/deallocate call lost the "any pending unpins?"
branch it used to execute first. The TSan reproducer now runs with zero reports.

---

## Bug 4 — `builtin_optional` was broken in four ways

**Severity: user-facing, non-concurrent. Affected: `density::optional`, the return
type of `function_queue::try_consume` for non-void signatures.**

`builtin_optional` (in `density_config.h`) had bugs that survived because templates
are only checked when instantiated, and the test suite never instantiated these
members:

1. **Copy assignment did not compile.** It did
   `swap(*this, builtin_optional(i_source))`, trying to bind a *temporary* to the
   non-const reference parameter of `swap`. The first user to write `a = b;` got a
   compile error. It is now the classic copy-and-swap: construct a named copy, swap
   with it.

2. **Both assignment operators had no `return *this;`.** Falling off the end of a
   function that returns a reference is undefined behavior — not a theoretical one: a
   `-O1` build of `b = std::move(a)` crashed with a segfault in our smoke test. The
   returns were added.

3. **`swap` leaked.** When swapping an engaged optional with an empty one, it
   move-constructed the value into the empty side but never destroyed the moved-from
   object on the other side — whose flag now said "empty", so its destructor never ran
   either. For a `TYPE` owning resources, that is a leak per swap. The moved-from
   object is now destroyed explicitly.

4. **The converting constructor was greedy, and `operator->` was const-broken.**
   `template <typename OTHER_TYPE> builtin_optional(OTHER_TYPE&&)` is a better match
   than the copy constructor for a *non-const lvalue*, so `builtin_optional b(a);`
   tried to construct `TYPE` from an optional and failed to compile; it is now
   constrained with `enable_if` to reject `builtin_optional` itself. And
   `TYPE * operator->() const` returned a non-const pointer from a const member — the
   const overload of the internal accessor returns `const TYPE *`, so this could never
   compile when used; there is now a proper const/non-const pair.

---

## Bug 5 — the flag bits were not actually reserved

**Severity: latent (harmless with default configuration).**

The low bits of `m_next` can hold flags only if every control block is aligned to more
than `LfQueue_AllFlags` (15) — i.e. to at least 16. The alignment constant was meant to
enforce that:

```cpp
s_alloc_granularity = max(destructive_interference_size, ...alignments...,
                          size_log2(LfQueue_AllFlags + 1));   // == 4 (!)
```

`size_log2(16)` is **4**, not 16 — the term contributed nothing. It only works today
because `destructive_interference_size` is 64. But that constant is documented as a
user-tunable configuration value; setting it below 16 (say, 8 for a small embedded
target) would silently make `LfQueue_InvalidNextPage` (8) collide with a real address
bit, corrupting every pointer with that bit set. The single-threaded `heter_queue`
already used the correct expression (`Queue_AllFlags + 1`).

The term is now `LfQueue_AllFlags + 1`, and a `static_assert` guarantees the invariant
regardless of future configuration changes.

---

## Bug 6 — a throwing singleton constructor hung every other thread

**Severity: low (needs an out-of-memory during startup).**

`SingletonPtr` guards the lazily-built global allocator state with a hand-rolled
ref-count protocol: 0 = not built, 1 = *being built* (everyone else spin-waits),
≥2 = built. If the singleton's constructor threw (the global state allocates memory,
so it can), the count was left at 1 forever: the exception propagated, and every other
thread spun for eternity on "another thread is constructing, yield and retry".

The construction is now wrapped in try/catch; on failure the count is restored to 0
before rethrowing, so another thread can retry the construction.

---

## Smaller fixes

- **`DENSITY_LIKELY` evaluated its argument twice.** It was defined as
  `(__builtin_expect((e), true), (e))` — the comma operator discards the hinted
  evaluation and evaluates `e` again (the second evaluation is the one the branch
  actually tests, so the hint was also inert). All current call sites were
  side-effect-free, but it was a footgun. It is now the standard
  `(__builtin_expect(!!(e), 1))`: one evaluation, working hint.

- **Integer overflow in `aligned_allocate` / `try_aligned_allocate`.** For
  over-aligned requests the functions compute `i_size + extra_size`; with an absurd
  `i_size` this wraps around, `operator new` succeeds with a tiny block, and the
  caller writes past it. The overflow is now detected (one compare on the cold,
  over-aligned path) and reported as `std::bad_alloc` / `nullptr`.

- **Function queues copied instead of moving the result.** The invoke-and-destroy
  thunks stored the callable's return value in `auto && ret` and then did
  `return ret;` — `ret` is an lvalue, so the result was *copied* out.
  It is now `return std::forward<RET_VAL>(ret);`, which moves for by-value signatures
  and leaves reference signatures untouched.

---

## A note on `enable_relaxed_atomics` (documentation change only)

All internal memory orders (`mem_relaxed`, `mem_acquire`, `mem_release`…) currently
map to `seq_cst` because `enable_relaxed_atomics` is `false`. Its comment used to say
the weaker orders were merely *untested* on weakly-ordered architectures. The
inspection showed they are **unsound as declared**, even in the abstract C++ memory
model (compiler reordering alone could break them on x86):

- every *pin-and-revalidate* handshake is a store→load pattern (Dekker-style): the pin
  increment must be globally visible before the validating load, and the deallocation
  side's zeroing/push must be visible before its pin-count check — that needs
  sequential consistency (or explicit fences), not `relaxed`/`acquire`;
- the multiple-consumer head reads `m_next` with `mem_relaxed` in several places where
  an *acquire* is required to see the contents of a newly linked zeroed page (the
  single-consumer head gets this right);
- on MSVC, `raw_atomic_load` with `memory_order_relaxed` compiles to a plain
  non-volatile load with no compiler barrier, so a spin loop of relaxed loads can be
  hoisted into an infinite loop.

The comment on the constant now says exactly this, so nobody flips the switch
expecting only "untested on ARM".

---

## How everything was verified

- **Smoke test** instantiating all queue types, plus the previously-broken
  `builtin_optional` operations.
- **Leak test:** 4M push/consume cycles per queue variant, watching RSS.
- **False-empty observer:** producers keep a provable surplus of committed elements
  while an observer hammers `empty()`; any "true" answer is a certified violation.
  111M checks across seq_cst / relaxed / sp after the fix: zero violations — including
  runs with an artificial delay injected into the exact race window.
- **Exactly-once invariant:** multi-producer/multi-consumer runs where the count and
  the sum of consumed payloads must match the produced ones exactly.
- **ThreadSanitizer** on the allocator pin/unpin reproducer: zero reports.
- **The project's own test suite**, built with `DENSITY_DEBUG` and
  `DENSITY_DEBUG_INTERNAL` (all internal assertions active): passes.
