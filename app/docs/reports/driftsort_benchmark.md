# DriftSort (Initial Implementation) Benchmark Results

**Date**: 2025-12-26
**Algorithm**: Adaptive Stable Mergesort (DriftSort logic)

## 5,000,000 Elements (Integers)

| Scenario | qsort (stdlib) | po_sort (DriftSort) | Speedup |
| :--- | :--- | :--- | :--- |
| **Random** | 0.7059s | 0.9023s | 0.78x |
| **Sorted** | 0.1523s | 0.0107s | 14.28x |
| **Reverse** | 0.1646s | 0.0265s | 6.22x |
| **Few Unique** | 0.3214s | 0.5497s | 0.58x |

## Observations

- **Adaptive Nature**: Significantly outperforms `qsort` (introsort) on sorted and reverse-sorted data due to run detection.
- **Random Overhead**: Slower on pure random data, likely due to stability overhead and `memcpy` costs in current generic implementation.
