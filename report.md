# Report

<!-- You should write your report in this file. Remember to check that it's 
     formatted correctly in the pdf produced by the CI! -->

## Overview

The implementation of malloc() I have developed includes certain optimisations that allow for efficient and dynamic memory allocation. It uses a best-fit strategy along with coalescing and fenceposts to limit fragementation and thereby optimising memory storage.
It first initialises the heap of 64Mb by using the function mmap, which in  my malloc is the primary memory pool for any requested allocations.
The biggest optimisation I have implemented is that my malloc() uses multiple free lists to adhere to varying block-size requests. This makes 
for faster access to the suitable-sized free blocks.

My Block datastructure forms the core of my malloc and it utilises metadata which consists of flags that rovide information on allocation status,
existence of fenceposts and whether the block has been allocated by mmap.
All of this metadata has been reduced to fit inside the last three bits of the header, hence saving memory that can be used for allocating more
data. 

Coming to coalescing, my malloc utilises boundary flags. The malloc checks adjacent blocks when a bolck becomes free (both prev and next) and
merges any contigous free block in to a large compound single block.
This way larger contigous blocks remain free for allocation.
Boundaries are marked by using fenceposts at the end and the start of the heap so that memory outside the allocated region is not accessed.
This data structure indicates clearly the heap's limits. 

Now when memory allocation requests that are bigger than what the memory we have access to can accomodate, i.e. it is greater than the heap size,
my malloc allows for dynamic-extension of the heap by adding on more memory from the OS by using mmap.


## Optimisations 

Metadata Reduction:
Cutting down on the memory used by each of the blocks, I implemented metadata reduction by putting together all the fields that were present
in the metadata as separate into one consolidated size field. So, I used the 3 least significant bits of the size fields to be repurposed into
encoding the allocation status (ALLOCATED_FLAG), fencepost status(FENCEPOST_FLAG) and mmap status(MMAP_FLAG). This reduced my overhead and my
allocator was able to use more of the heap to store data.

Constant Time Coalescing:
This optimisation was done to prevent or reduce fragmentation, this is done by combining adjacent free blocks to form longer
and bigger contigous block. This results in larger contigous blocks of memory hence reducing fragmentation as mentioned in the assignment.
I have implemented constant time coalescing by using the prescirbed boundary tags, which come as storing data both at the beginning(header) and 
at the end(footer) for each memory block. The footer of every block is the same as the header's size and info.
The allocator, when a block is freed is able to instantly access the neigbours by referencing the footer of the previous block and the header
of the next block. This direct access completely voids the need to go through the entire heap to find neighbouring blocks. This prevents the 
O(n) complexit and optimises it to O(1).

Requesting additional chunks from the OS:
This optimisation is done to handle cases when allocation requests are unable to be accomodated by the initalised heap.

There are 2 primary conditions, which are listed as follows,

Exceeding Heap Inital Capacity:
When the memory allocation request exceeeds the remaining space in the 64Mb heap, the allocator calls mmap again to obtain a new memory region.
This prevents the possibility of the large request from causing fragementaton and makes for the original memory region being free for 
shorter and more frequent allocations.

Handling Large Allocation Requests:
This is for allocation requests that are inherently large. For this case my malloc bypasses the available free lists and goest directly to 
allocate the request by calling mmap. 

The additional memory regions requested from mmap follow the same structure as the initial heap but doesn't exist in list form, it is 
managed separately.

The alignment function was used to maintain compliance for the extra memory with the rest of the heap in the malloc.


Multiple Free Lists:
The multiple free list optimisation categorises blocks into size classes. 

This stratification allows for several advantages:

Enhanced Allocation Speed - Due to having different size class based lists, traversing one list to find a free block big enough isn't required and
a targeted search can be carried out to access the relevant free list thereby reducing the time complexity.

Reduced Memory Fragmentation - By segregating blocks of different sizes, coalescing takes place more efficiently thereby reducing both internal 
and external fragmentation. It also contributes to a high utilisation rate and longevity of the heap.

Improved Scalability - As allocations grow it can bottleneck, leading to degraded performance, having multiple lists distributes workload across
the different size based classes.

Superior Best-Fit Selection - Multiple free lists paves the path for a more suitable and refined best-fit strategy within each size class. 
This is due to the different size-based classses as it makes for minimal overhead.


Enhanced Parallelism Potential
Having multiple free lists can enhance opportunities for parallel processing and concurrency optimizations. By managing different 
size classes separately, it becomes possible to perform searches and modifications in parallel without running into contention issues.


## Testing

The biggest challenge I faced while making this malloc was the implementation of fenceposts and coalescing to prevent fragmentation.
This issue manifested itself as memory leaks with extremely prevalent segmentation faults. Running the tests always lead to the address sanitiser giving a big error which was hard to decode. It caused the tests Simple2 and fragementation to fail. Sometimes, the fragmentation one would pass
but not consistently. Upon searching through debugging documentation I nailed down the root cause to bee incorrect updating of my free list pointers
during the coalescing process, which in large disrupted the integrity of my free lists and prevented the proper merging of blocks.
This resulted in the absence of longer contigous blocks of memory being formed.
To solve this I traced down the processing of my pointer and tracked their path and installed small checks wherein I could ensure they were being
transferred properly and I also added a function to validate pointers.
The designing and integration of fenceposts into the malloc was also a hefty task an I had to format their start and end functions while ensuring
they did not interfere with the allocation and deallocation processes. 
Specifically when attempting to merge free blocks the malloc oocasionally tried to access memory from  outside the fenceposts, leading to the 
segmentation faults mentioned earlier.To address this, I put in place strict checks to make sure that coalescing operations adhered to the limits set by the fenceposts:   With these boundary checks in place, the allocator was able to merge free blocks safely without breaching the heap boundaries.
Running the tests and examining their functionality really helped me to resolve these errors.

## Benchmarking

The malloc went under the benchmarking tests provided and yielded great results, I was able to reduce the average time from about 1.4 sec in my
first iteration to only 0.076 sec in my final implementation.
The optimisation that improved m results significantly was constant time coalescing which pretty much led to a tenfold decrease in the benchmarking.
The uniformity of my results also indicate the consistency in terms of how memory is allocated dynamically and efficiently.

Peak Memory Utilization Metric:
To gain deeper insight into my allocator's management capabilities, I implemented a peak utilisation metric function as described in the lecture slides. This was done by tracking the payload of allocated memory and identifying the peak usage during benchmark runs.

The following are my tracking variables,
current_memory_usage
peak_memory_usage

These metrics are updated in both my_malloc() and my_free(). Incrementing in malloc and decrementing in free respectively.

A fragmentation script to test randomised allocations and deallocations was put in place in fragementation.c which tracked the current_payload
and updated max_payload whenever a new peak was reached.

The fragmentation tests revealed a peak memory utilization (Uk) of 0.2162%, relative to the total heap size of 67,108,864 bytes.

This super low fragmentation rate shows just how good the allocator is at keeping big chunks of memory together, which cuts down on wasted space and boosts memory efficiency. By effectively merging free blocks through smart pointer management and fencepost techniques, it makes sure memory is used to its fullest, keeping performance high even when there are a lot of allocations and deallocations happening.

I tried to benchmark my results against a lower iteration but I hadn't saved my previous work and wasn't able to remove the multiple free list 
optimisation with enough time to test against it.

But I do remember the results of my implementation and they were aprooximately 1.08 sec but other programs on my machine were also running during that period hence that could also be the reason for this big of a performance upgrade.