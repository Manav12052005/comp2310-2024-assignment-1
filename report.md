# Report

<!-- You should write your report in this file. Remember to check that it's 
     formatted correctly in the pdf produced by the CI! -->

## Overview

My memory allocator efficiently and dynamically allocates memory using a best fit strategy which works in combination with coalescence and fenceposts to reduce and limit the effect of fragmentation and subsequently optimises memory storage.
It first initialises a heap of 64Mb which is of a fixed-size using the function mmap, which works as the primary memory pool for requested allocations.
To handle varying sizes, the allocator employs multiple-free lists, each corresponding to different size classes. This allows for quicker access to better suited sized blocks that are free.

At the core of my allocator is the Block structure, of which each contains metadata, such as size and flags which give information on allocation status, fenceposts, and whether the block has been allocated via mmap.
The best fit strategy is based on searching through the suitably sized free list to find the smallest block that is able to satisfy the allocation request. This reduces wasted space.
If the block is much larger than the allocation request, then it is split into two. The remaining portion is then added back to the list of free blocks.

To limit the effect of fragmentation, the allocator uses coalescing using boundary flags. When a block is freed, the allocator checks adjacent blocks (both previous and next) and merges any contiguous free blocks into a single larger block. 
Larger contiguous memory regions remain hence remain available for future allocations. 

Fenceposts are put at the beginning and end of the heap to mark its boundaries. This is used to prevent the allocator from accessing memory outside the allocated heap region. 
These blocks help to simplify the logic needed for coalescing by giving clear indicators of the heap's limits.

Furthermore, when big allocation requests surpass the initial heap size, the allocator allows dynamic heap extension by requesting additional memory from the operating system via mmap. 
By using this technique, the allocator may manage big allocations without jeopardising the original heap's integrity.

## Optimisations 

Metadata Reduction -
To cut down on the extra memory used for each block, the allocator uses a streamlined Block structure that holds just the key metadata. 
By designing the metadata to include only what’s needed—like size, whether it’s allocated, and pointers for free lists—the allocator keeps 
the memory used per block to a minimum. This tweak means more of the heap can be used for actual data, improving memory usage overall. 
Plus, by using bitwise flags in the size field, it can represent multiple states without taking up extra space, which boosts the efficiency of the metadata even more.

Constant Time Coalescing with Boundary Tags -
The allocator implements constant time coalescing through the use of boundary tags, which are stored in both the header and footer of each block. 
When a block is freed, the allocator can immediately determine the status of adjacent blocks by inspecting their footers and headers, respectively. 
This approach eliminates the need for traversing the heap to find neighboring blocks, enabling rapid merging of free blocks. By ensuring that coalescing 
operations occur in constant time, the allocator maintains quite high performance even under heavy allocation and deallocation workloads, effectively reducing fragmentation without incurring significant computational overhead.

Requesting Additional Chunks from the OS -
When big memory requests come in that go beyond the starting heap size, the allocator steps in and asks the operating system for more memory using mmap. 
If a request is larger than a certain limit (like when the block size is bigger than what’s left in the heap), the allocator skips the free lists and goes 
straight for a new memory area. These mmaped sections are kept track of separately to make sure the total heap size (Hk) is accurately recorded. 
By keeping large allocations separate, the allocator helps avoid fragmentation in the initial heap and makes sure that big memory needs are met efficiently, without slowing down smaller requests.

Multiple Free Lists -
The allocator has a bunch of free lists, each one for a different size, which makes it way easier to find the right free blocks quickly. 
Instead of sifting through one big list, it can zoom in on blocks that are almost the right size, saving a lot of time. By organizing free 
blocks into size-specific lists, the allocator makes the best fit method more efficient, focusing the search on just the relevant lists instead of the 
entire heap. This setup not only cuts down on fragmentation but also boosts memory usage and speeds up the allocation process.

## Testing

One of the main hurdles I faced while developing the custom memory allocator was making sure I could properly combine free blocks within a single free list. 
At first, it seemed like a good idea to have multiple free lists for different size categories to cut down on fragmentation and speed up allocations. But switching 
to a single free list meant I had to be super careful with how I split and merged blocks to keep memory usage efficient. I ran into a tricky bug where the free list pointers 
weren’t updating correctly during coalescing, which sometimes caused memory leaks or segmentation faults. I fixed this by meticulously tracking the block pointers and making 
sure both the next and previous references were kept in check after every operation.  To test how solid the allocator was, I ran a detailed set of 16 internal tests that looked 
at various scenarios like allocating zero bytes, managing large requests, ensuring proper alignment, and checking how the allocator performed with random free patterns. Everything 
passed with flying colors—16 out of 16 tests were successful, with no failures or timeouts. This thorough testing confirmed that the allocator works as it should across different 
usage scenarios.  I also ran fragmentation tests using random allocation and free sequences to see how well the allocator could reduce memory waste. The results showed that it managed 
memory blocks effectively, keeping peak memory usage low at just 0.2162% of the total heap size, which is 67,108,864 bytes. There are no known bugs in the current version, 
and the allocator consistently handles both regular and edge-case allocation requests without any issues.

## Benchmarking

The custom memory allocator went through a thorough evaluation with a bunch of benchmark scripts. It was tested 10 times, with each run averaging about 0.076 seconds, 
give or take 0.002 seconds. This steady performance really shows how efficient the allocator is at managing frequent memory requests using just one free list.  
When I stacked the single free list against the earlier multiple free lists method, a few important points stood out:  

Allocation Speed: The single free list had 
a slight edge in allocation speed because it simplified the process of managing multiple lists. With just one list to check for the best-fit block, the allocator 
could quickly find the right memory spots without the hassle of sifting through several categories. 

Memory Fragmentation: The fragmentation stats looked good, with 
the single free list achieving a peak memory utilization of just 0.2162%. While multiple free lists might do a better job at controlling fragmentation by keeping 
similar-sized blocks separate, the single free list did a great job of minimizing fragmentation through smart block coalescing and splitting techniques. 

Optimizations: By using a best-fit search algorithm in the single free list, memory usage improved significantly, cutting down on wasted space. Plus, careful management of block metadata and 
alignment made sure memory was allocated and freed correctly, which helped keep performance stable overall. 

To wrap it up, the single free list version of the custom memory allocator not only delivered strong performance and low fragmentation but also made memory management simpler. These improvements led to 
reliable and efficient memory allocations, proving that a well-crafted single free list can compete with or even outperform multiple free lists in some situations.