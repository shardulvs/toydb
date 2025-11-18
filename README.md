# ToyDB Storage Engine — README

This project implements the three major lower layers of a simplified Database Management System (DBMS):

1. PF Layer: Paged File with Buffer Pool and Page Replacement
2. SP Layer: Slotted Pages for Variable-Length Records
3. AM Layer: B+ Tree Index Construction and Querying

The repository is organized as follows:

```
toydb/
 ├── pflayer/     # PF layer and slotted-page layer
 └── amlayer/     # AM layer and indexing tests
```

Each objective includes test programs and produces CSV files for performance evaluation.

---

# Objective 1 — PF Layer with Buffer Management

## Features Implemented

* Buffer pool with configurable size.
* Page replacement policies: LRU and MRU.
* Page pinning and unpinning with dirty-bit tracking.
* Statistics counters:

  * logicalPageRequests
  * physicalReads
  * physicalWrites
* A workload generator to test performance under different read/write ratios.

## Running PF Layer Tests

```
cd pflayer
make clean
make tests
./test_pf_experiments
```

## Output

* Terminal output showing performance for 100% read down to 0% read.
* Results saved to:

```
pflayer/pf_results.csv
```

---

# Objective 2 — Slotted-Page (SP) Layer

The SP layer implements a slotted-page structure for storing variable-length records. It provides:

* Record insertion (`SP_InsertRecord`)
* Record deletion (`SP_DeleteRecord`)
* Record retrieval (`SP_GetRecord`)
* Page compaction (`SP_CompactPage`)
* Sequential scanning (`SP_ScanNext`)
* Space utilization measurement (`SP_ComputeSpaceUtilization`)

A test program inserts all records from the provided student dataset into a slotted-page file and compares utilization with static fixed-length record layouts.

## Running Slotted-Page Tests

```
cd pflayer
./test_sp
```

## Output

* Slotted-page file:

```
pflayer/sp_student.dat
```

* Static vs slotted-page utilization results written to:

```
pflayer/sp_results.csv
```

---

# Objective 3 — AM Layer: B+ Tree Indexing

The AM layer already provides the B+ tree implementation required by the assignment. To evaluate indexing performance, three index construction methods were implemented:

1. Build-from-file method
2. Incremental insert method
3. Bulk-load (sorted) method

A query driver is also provided to measure point and range query performance.

## Important: Copy slotted-page file before running AM tests

The AM programs run inside the `amlayer/` directory, so the `sp_student.dat` file must be copied there from the `pflayer/` directory:

```
cp pflayer/sp_student.dat amlayer/
```

Without this step, the AM drivers will not find the input data file.

---

# Building AM Layer and Objective 3 Test Programs

```
cd amlayer
make clean
make tests
```

This builds the following executables:

* `build_from_file`
* `build_incremental`
* `bulk_load_index`
* `test_queries`

---

# Running the Three Index-Build Methods

Each index-build program is invoked as:  `./program  <sp-file>  <indexNo>  <fieldIndex>`
where `<indexNo>` chooses the output index file (student.<indexNo>)
and `<fieldIndex>` specifies which semicolon-separated field contains the roll number key.

## 1. Build-from-file

```
cd amlayer
./build_from_file sp_student.dat 1 1
```

Produces:

```
am_build_from_file.csv
student.1
```

## 2. Incremental index construction

```
./build_incremental sp_student.dat 2 1
```

Produces:

```
am_build_incremental.csv
student.2
```

## 3. Bulk-load (sorted keys)

```
./bulk_load_index sp_student.dat 3 1
```

Produces:

```
am_bulk_load.csv
student.3
```

---

# Query Testing

The query tester is invoked as:  `./test_queries <indexNo> <queryType> <value(s)>`
where `<queryType>` is either `point <key>` or `range <low> <high>` and indexNo selects the index file student.`<indexNo>`.

```
./test_queries 3 point 95302001
./test_queries 3 range 900000 990000
```

Output is written to:

```
am_query_results.csv
```

This includes:

* Query execution time
* Logical page requests
* Physical reads
* Physical writes
* Number of results returned

---

# Output Files Summary

After all tests, the following CSV files will be generated:

| Layer              | CSV File                           |
| ------------------ | ---------------------------------- |
| PF Layer           | `pflayer/pf_results.csv`           |
| SP Layer           | `pflayer/sp_results.csv`           |
| AM Build-from-file | `amlayer/am_build_from_file.csv`   |
| AM Incremental     | `amlayer/am_build_incremental.csv` |
| AM Bulk-load       | `amlayer/am_bulk_load.csv`         |
| AM Query Tests     | `amlayer/am_query_results.csv`     |

---

# Summary of Achievements

All three objectives were successfully implemented:

1. PF Layer extended with buffer management, page replacement policies, and detailed I/O metrics.
2. Slotted-page layer developed for variable-length records, with analysis comparing slotted-page vs static layouts.
3. B+ tree index constructed using three different methods, with performance measurements for both build and query operations.

This project demonstrates a working miniature DBMS storage stack and provides empirical analysis across multiple workload patterns and indexing strategies.

---
