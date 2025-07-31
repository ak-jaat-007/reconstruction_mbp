# MBP‑10 Order Book Reconstruction

## Overview

This project reconstructs the MBP‑10 (Market By Price, top 10 levels) order book from Databento’s MBO (Market By Order) input data. It reads `mbo.csv` (input), processes order book updates (add, modify, cancel, clear, trade/fill), and outputs `mbp.csv` in the exact format required.

### Key features:

- Ignores the initial `R` (clear) event at start of day.
- Merges `T` (Trade), `F` (Fill), and `C` (Cancel) into one synthetic `T` event when applicable.
- Skips trades with `side = N` (no effect on book).
- Outputs top 10 bid/ask levels on every change.
- Prices have 9 decimal places and match the sample `mbp.csv` column order.

---

## Build Instructions

Ensure you have `g++` (MinGW) and `mingw32-make` installed.

In the project root, run:

```bash
mingw32-make
```

This creates the executable:

```
reconstruction_mbp.exe
```

---

## Run Instructions

Run the executable with the input CSV:

```bash
reconstruction_mbp.exe mbo.csv
```

- **Input**: `mbo.csv` (Databento MBO data)
- **Output**: `mbp.csv` (reconstructed MBP‑10 data)

> The input file must be in the same directory as the executable (or provide full path).

---

## Output Format

- Column order matches Databento MBP‑10 spec.
- Prices use 9 decimal places (e.g., `5.510000000`).

---

## Optimizations

### Efficient Data Structures

- `std::map<double, Level, std::greater<double>>` for bids (highest price first)
- `std::map<double, Level>` for asks (lowest price first)
- `std::unordered_map<std::string, OrderInfo>` for order lookup by ID

→ Fast `O(log n)` for price levels and `O(1)` for orders.

### Event Handling

- Processes A/M/C/R directly into the book.
- Combines T/F/C sequences into one synthetic T snapshot.
- Skips N side trades.

### Performance

- Single-pass streaming using a lightweight FastCSV parser.
- Avoids sorting; top 10 levels derived directly from map iterators.
- Outputs only when book changes.

---

## Repo Contents

- `main.cpp` – Main driver
- `order_book.cpp / .hpp` – Core book logic
- `mbp_writer.cpp / .hpp` – Output writer
- `fast_csv.hpp` – Lightweight CSV parser
- `Makefile` – Build instructions
- `reconstruction_mbp.exe` – Prebuilt executable
- `Readme.txt` – This file
- `.gitignore` – Ensures no CSV or build artifacts are pushed

> Not included: `mbo.csv` and `mbp.csv` (due to size and privacy)

---

## Sample Run

```bash
mingw32-make
reconstruction_mbp.exe mbo.csv
```

**Reconstruction complete. Output: `mbp.csv`**
