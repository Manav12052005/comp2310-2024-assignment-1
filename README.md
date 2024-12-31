## Overview

The implementation of `malloc()` developed for this project includes several optimizations to ensure efficient dynamic memory allocation. The allocator uses a best-fit strategy, coalescing, fenceposts, and multiple free lists to minimize fragmentation and improve memory utilization. The heap is initialized with 64 MB using `mmap`, which serves as the primary memory pool for allocation requests. Larger allocations exceeding the heap capacity dynamically extend the heap by additional `mmap` calls.

### Key Features

1. **Metadata Optimization**:
   - Metadata fits within three bits in the header, encoding allocation status, fencepost status, and `mmap` status. This reduces overhead and increases memory availability.

2. **Constant-Time Coalescing**:
   - Adjacent free blocks are merged efficiently using boundary tags. Each block’s header and footer store metadata, allowing the allocator to directly access neighboring blocks without traversing the entire heap. This ensures coalescing operates in constant time (O(1)).

3. **Fenceposts**:
   - Fenceposts mark the boundaries of the heap, preventing out-of-bounds memory access during allocation and deallocation.

4. **Dynamic Heap Extension**:
   - Large allocation requests or heap overflows trigger `mmap` to extend memory dynamically, ensuring scalability and preventing fragmentation.

5. **Multiple Free Lists**:
   - Free blocks are categorized into size-based lists. This improves allocation speed by enabling targeted searches, reduces fragmentation by efficient coalescing within classes, and enhances scalability by distributing workloads.

---

## Optimizations

1. **Metadata Reduction**:
   Consolidating metadata into the size field saved memory, allowing more heap space for data storage.

2. **Coalescing**:
   Boundary tags enabled constant-time merging of adjacent free blocks, reducing fragmentation and ensuring larger contiguous memory blocks remain available.

3. **Dynamic Allocation**:
   Large allocation requests bypass the free lists and directly use `mmap`, maintaining the original heap for smaller allocations.

4. **Multiple Free Lists**:
   Categorizing blocks by size classes improved allocation speed and reduced fragmentation, enhancing heap utilization and scalability.

---

## Testing

Challenges arose in implementing fenceposts and coalescing, resulting in memory leaks and segmentation faults. Incorrect pointer updates during coalescing disrupted free list integrity, preventing proper merging of blocks. Debugging revealed the need for boundary checks to ensure safe operations within fencepost limits. Adding validation functions and strict pointer checks resolved these issues, stabilizing the allocator.

Tests, including “Simple2” and “Fragmentation,” validated functionality, and consistent success was achieved after debugging.

---

## Benchmarking

Performance improved significantly across iterations:

- Initial average time: 1.4 seconds
- Final optimized time: 0.076 seconds

**Key Improvements:**
- Constant-time coalescing drastically reduced runtime.
- Multiple free lists streamlined allocation, minimizing search overhead.

**Fragmentation Results:**
A fragmentation test revealed a peak memory utilization of **0.2162%** relative to a 64 MB heap. Effective coalescing and boundary management minimized wasted space, showcasing high efficiency even under randomized allocation and deallocation scenarios.

**Peak Utilization Metrics:**
- `current_memory_usage`: Tracks live memory payload.
- `peak_memory_usage`: Captures highest usage during execution.

These metrics highlighted consistent performance and efficient memory management, with low fragmentation rates demonstrating allocator robustness.

---

## Conclusion

This `malloc()` implementation balances speed and memory utilization through optimizations like metadata reduction, constant-time coalescing, dynamic heap extension, and multiple free lists. Benchmark results and fragmentation tests affirm its efficiency, scalability, and minimal fragmentation. The allocator effectively handles diverse allocation patterns while maintaining high performance and memory efficiency.
