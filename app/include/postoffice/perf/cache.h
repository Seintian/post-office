/**
 * @file cache.h
 * @brief Cache line size constants for preventing false sharing
 * 
 * This header defines portable cache line size constants used throughout
 * the codebase to add padding to structures accessed by multiple threads.
 * 
 * @see false_sharing_bench.c for performance impact demonstration
 */

#ifndef POSTOFFICE_PERF_CACHE_H
#define POSTOFFICE_PERF_CACHE_H

/**
 * @brief Conservative maximum cache line size for portability
 * 
 * Different CPU architectures have different cache line sizes:
 * - x86-64 (Intel, AMD): 64 bytes
 * - ARM64 (Apple M1/M2, AWS Graviton): 64 bytes
 * - PowerPC (some models): 128 bytes
 * - Future architectures: potentially 128+ bytes
 * 
 * We use 128 bytes as a conservative maximum to ensure correctness
 * across all platforms. This wastes memory on x86-64/ARM64 but
 * guarantees no false sharing on any architecture.
 * 
 * Memory vs Performance Tradeoff:
 * - Cost: ~64 bytes extra per padded field on x86-64
 * - Benefit: 20-40% performance improvement in multi-threaded scenarios
 * 
 * @note Use this for structures with few instances (< 100)
 * @note For high-volume structures, consider platform-specific sizing
 */
#define PO_CACHE_LINE_MAX 128

#endif /* POSTOFFICE_PERF_CACHE_H */
