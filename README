# Dynamic Memory Manager

This is a memory allocator which replaces all calls to C library 
functions: malloc, calloc, realloc and free.

KNOWN PROBLEMS:
----------------

None that I have found, all tests ran sucessfully. also checked with memset and it 
works fine.

DESIGN:
--------

The allocator was designed using some code from a previous semester, such as the 
Mem_* funuctions. These are basically helpers that malloc, calloc, etc. call to 
get the memory blocks. 

The allocator pulls memory blocks from freelists depending on the size of the request.
These lists are split up by a power of 2 with the lowest size being 16 bytes. This is 
because I use a structure (chunk_t) to carve the pages from mmap into and that struct
is 16 bytes (system shouldn't matter because 1. the  program will handle it via extra
lists or 2. padding will make  it 16 bytes anyways). Anything larger will be given it's
own page. Each list initially has a header block which will point to any pages that are
added. 

Ignore the policies at the top, that was for explicit free lists and effiecieny tests. 
This implimentation uses First fit because all memory is assigned in 
perfect blocks(powers of 2) depending on the request. Also Coalescing is off for speed
since all blocks are the same, it doesn't matter.
