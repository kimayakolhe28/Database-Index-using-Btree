# B+ Tree File Management System

A **high-performance, in-memory file index** built on a B+ tree — the same
data structure that powers NTFS, ext4, and most production databases.

---

## Features

| Feature | Details |
|---------|---------|
| **Exact search** | O(log n) — ~11 comparisons for 1 000 files |
| **Range queries** | Scan from any filename to any other alphabetically |
| **List by extension** | Find all `.pdf`, `.c`, `.json`, … files instantly |
| **Directory listing** | Filter by path prefix |
| **Real directory indexing** | Walk your actual filesystem and index every file |
| **Persistent index** | Save / load the index to/from a flat text file |
| **Performance benchmark** | Built-in comparison against linear search |
| **Interactive demo** | Full menu-driven CLI |

---

## Quick Start

```bash
# Build everything
make

# Run the interactive file manager (loads 51 sample files automatically)
./bin/file_manager

# Run the test suite (27 tests)
make test
```

---

## Project Layout

```
file_btree/
├── include/
│   └── file_btree.h        # All structs, constants, and API declarations
├── src/
│   ├── btree_core.c        # Tree creation, stats, visualisation, persistence
│   ├── btree_insert.c      # Top-down pre-splitting insertion
│   ├── btree_search.c      # Exact search, range, list-by-type, dir indexing
│   ├── btree_delete.c      # Deletion with borrow / merge rebalancing
│   ├── btree_bench.c       # Performance benchmarks & CSV export
│   └── main.c              # Interactive CLI application
├── tests/
│   └── test_suite.c        # 27-test suite covering all operations
├── output/                 # Generated: autosave.idx, benchmark_results.csv
└── Makefile
```

---

## B+ Tree Configuration

Defined in `include/file_btree.h`:

```c
#define MIN_DEGREE  3          // t = 3
#define MAX_KEYS    5          // 2t - 1
#define MAX_CHILDREN 6         // 2t
```

- **All file data lives in leaf nodes only** — internal nodes hold only
  routing keys.
- **Leaves are singly linked** — this makes range queries and sorted
  listing a simple sequential scan after reaching the first matching leaf.
- **Top-down pre-splitting** — nodes are split *before* descending, so
  insertion never needs to walk back up the path.

---

## Menu Reference

```
1  Add / update a file          — insert by filename, path, size
2  Search for a file            — exact lookup; reports µs and comparisons
3  Delete a file                — removes from index, rebalances
4  List all files (sorted)      — full leaf-chain scan
5  List files by extension      — e.g. "pdf", "c", "json"
6  List files in directory      — filter by path prefix
7  Range search [start … end]   — alphabetic range, e.g. "a" to "m"
8  Index a real directory       — walks your filesystem recursively
9  Save index                   — writes flat CSV-like text file
10 Load index                   — rebuilds tree from saved file
11 Show tree structure          — ASCII visualisation of nodes
12 Show statistics              — height, node count, utilisation
13 Run performance benchmark    — B+ tree vs linear, exports CSV
14 Load sample dataset          — loads the built-in 51-file demo set
 0 Exit                         — auto-saves to output/autosave.idx
```

---

## Performance Results

Search benchmark (2 000 random queries per dataset size):

| Dataset | B+ Tree (ms) | Linear (ms) | Speedup | Avg Comparisons |
|--------:|------------:|------------:|--------:|----------------:|
|     500 |       <1    |        <1   |   ~10×  |            9.8  |
|   1 000 |       <1    |        10   |  >10×   |           11.1  |
|  10 000 |       <1    |        60   |  >60×   |           14.8  |
|  50 000 |       10    |       660   |   66×   |           17.1  |

**Tree height growth** (logarithmic):

| Files  | Height | Nodes  |
|-------:|-------:|-------:|
|     10 |      2 |      4 |
|    100 |      4 |     48 |
|  1 000 |      6 |    497 |
| 10 000 |      8 |  4 996 |
| 50 000 |     10 | 24 994 |

Height = O(log_t n).  With t = 3 and 50 000 files the tree is only 10
levels deep — meaning a search touches at most 10 nodes.

---

## Key Algorithms

### Search — O(log n)
Navigate internal nodes by comparing the target filename against routing
keys.  On reaching a leaf, perform a linear scan (≤ MAX_KEYS = 5 items).

### Insert — O(log n), top-down pre-splitting
1. If the root is full, split it and create a new root.
2. Descend toward the target leaf; pre-split any full child encountered
   so the parent always has room.
3. Insert into the (guaranteed non-full) leaf in sorted order.

### Range query — O(log n + k)
Find the leftmost leaf via normal descent, then follow the `next` pointer
chain across leaves until the end key is exceeded.  Each file is visited
exactly once; no backtracking.

### Delete — O(log n)
Descend to the target leaf and remove the entry.  On underflow:
1. Borrow from the right sibling (update parent separator key).
2. Borrow from the left sibling.
3. Merge with a sibling; remove the separator from the parent.
Propagate root collapse if the root becomes empty.

---

## Data Structures

```c
typedef struct FileEntry {
    char     filename[256];   // indexed key
    char     path[512];       // full path stored in leaf
    long     size;
    char     type[16];        // extension (e.g. "pdf")
    time_t   created, modified;
} FileEntry;

typedef struct BPlusNode {
    char       keys[MAX_KEYS][256];        // routing / leaf keys
    BPlusNode *children[MAX_CHILDREN];     // NULL in leaf nodes
    FileEntry *files[MAX_KEYS];            // NULL in internal nodes
    BPlusNode *next;                       // leaf linked list
    int        num_keys;
    bool       is_leaf;
} BPlusNode;
```

---

## Real-World Relevance

The same B+ tree structure is used by:

- **NTFS** (Windows file system) — directory indexing
- **ext4** (Linux) — HTree directory indexing
- **InnoDB / MySQL** — primary and secondary indexes
- **PostgreSQL** — default index type
- **SQLite** — table B-tree and index B-tree

---

## Building & Testing

```bash
make           # build bin/file_manager
make test      # build and run bin/run_tests (27 tests)
make clean     # remove obj/, bin/, output/
make rebuild   # clean then build
```

Compiler: GCC, standard C11.  No external dependencies.
