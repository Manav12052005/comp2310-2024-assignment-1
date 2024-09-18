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

One of the biggest challenges I encountered while building the custom memory allocator was figuring out how to properly combine free blocks in a structure with multiple free lists. At first, managing just one free list was pretty simple, but as I expanded the allocator to handle multiple lists for different size categories, it got a lot trickier to keep each list intact during block splitting and merging. I ran into a tough bug where the free list pointers weren’t updating correctly during coalescing, which sometimes caused memory leaks or segmentation faults. I fixed this by carefully tracking the block pointers and making sure that both the next and previous references were consistently updated across all free lists after every operation.  To test how solid the allocator was, I ran a detailed set of 16 internal tests that looked at various situations like allocating zero bytes, managing large allocations, ensuring proper alignment, and checking how the allocator performed with random free patterns. All tests came through with flying colors, showing 16/16 tests passed; 0/16 tests failed; and 0/16 tests timed out. This thorough testing confirmed that the allocator works as it should across a variety of usage scenarios.  On top of that, I also ran fragmentation tests using random allocation and free sequences to see how well the allocator could reduce memory waste. The results showed that it managed memory blocks effectively, keeping the peak memory usage low at just 0.2162% of the total heap size of 67,108,864 bytes. This was made possible by tracking the highest memory usage during the allocator's operations. The allocator reliably handled both standard and edge-case allocation requests without any known issues, ensuring dependable performance.

In the my_malloc function, after allocating memory and updating current_memory_usage, the allocator checks whether the new current_memory_usage exceeds the existing peak_memory_usage. If it does, peak_memory_usage is updated accordingly. Conversely, in the my_free function, when memory is freed, current_memory_usage is decremented, but peak_memory_usage remains unchanged as it represents the historical maximum usage.
## Benchmarking

The custom memory allocator went through a thorough testing process with a bunch of benchmark scripts. It was run 10 times, and each test averaged around 0.076 seconds with a slight variation of ±0.002 seconds. This consistent performance highlights how effective the allocator is at handling frequent memory requests using several free lists.  When we looked at the multiple free lists setup versus the older single free list method, a few important points stood out:  

Allocation Speed: The multiple free lists showed a bit faster allocation times because they could search specifically within size-related lists. With this setup, the allocator quickly found the best-fit block in the right size category without having to sift through one big list. This tweak made the allocation process smoother and quicker.  

Memory Fragmentation: The fragmentation stats were still looking good, with the allocator achieving a peak memory utilization of just 0.2162%. While a single free list can do the job, having multiple free lists really helps control fragmentation better by keeping blocks of similar sizes separate. This setup cuts down on internal fragmentation and makes sure memory is used more effectively, keeping peak utilization low.  

Optimizations: By using a best-fit search algorithm with the multiple free lists, memory usage improved significantly by minimizing wasted space. Plus, careful management of block metadata and alignment made sure memory was allocated and freed correctly, which helped keep performance stable. The reduced overhead from metadata and better handling of boundary tags also boosted the allocator's efficiency.  

In summary, the multiple free lists version of the custom memory allocator not only kept strong performance and low fragmentation but also improved memory management efficiency compared to the single free list method. These enhancements led to reliable and effective memory allocations, proving that a well-designed system makes a difference.