#define _GNU_SOURCE

void __attribute__ ((constructor)) shim_init(void);

/* mem.c
 *
 * Project 3
 * Harsh Patel
 * ECE 3220, Spring 2017
 * 
 * This is the dynamic memory management package
 * the user will call malloc, free, realloc and calloc
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>


#define PAGESIZE 4096

#define LISTSIZE 11

#define FIRST_FIT  0x1 
#define BEST_FIT   0xB
#define WORST_FIT  0xF
#define TRUE 1
#define FALSE 0

/* must be FIRST_FIT or BEST_FIT or WORST_FIT */
int SearchPolicy = FIRST_FIT; // this is used because all blocks are same size

/* TRUE if memory returned to free list is coalesced */
int Coalescing = FALSE; // faster with this off

typedef struct chunk_tag {
    struct chunk_tag *next;
    short size;
} chunk_t;

// private function prototypes
void mem_coalesce(chunk_t*, int);
void *Mem_alloc(const int nbytes, int);
void Mem_free(void *return_ptr, int);
chunk_t *morecore(int new_bytes); 

// Global variables required in mem.c only
static chunk_t Dummy_List[LISTSIZE];
static chunk_t * Rover_List[LISTSIZE];
static int sbrkCalls = 0;
static int numPages = 0;


// Constructor
void shim_init(){
   int i;
   for(i = 0; i < LISTSIZE; i++){
      Dummy_List[i] = (chunk_t){&Dummy_List[i], 0};
      Rover_List[i] = &Dummy_List[i];
   } 
}


void assignList(int size, int* i){
   *i = 0; // 2^4 or sizeof(Chunk_t)
   int tempSize = size;
   while(tempSize >>= 1) { *i += 1; }
   if(*i < 4) *i = 4; 
   if((1 << *i) < size) (*i)++;
}

// Malloc override
void * malloc(size_t size){
   if(size > PAGESIZE/4){
      int newsize = size + sizeof(chunk_t);
      if(newsize % sizeof(chunk_t) != 0)
         newsize += sizeof(chunk_t) - (newsize % sizeof(chunk_t));
      chunk_t* temp = morecore(newsize);

      if(temp == NULL) exit(-1);

      temp->size = (newsize)/sizeof(chunk_t);
      numPages -= (newsize)/PAGESIZE;
      return (void*)(temp+1);
   }

   int i;
   assignList(size, &i);
   int blockSize = (1<<i);
   return Mem_alloc(blockSize, i);
}


// Calloc override
void * calloc(size_t nitems, size_t size){
   void* ret =  malloc(nitems*size);
   memset(ret, 0, nitems*size);
   return ret; 
}

// Override Free
void free(void * ptr){
   if(ptr == NULL) return;

   chunk_t* optr = (chunk_t*)ptr;
   optr--;
   int size = optr->size * sizeof(chunk_t);
   size -= sizeof(chunk_t);
   if(size > PAGESIZE/4){
      munmap((void*)optr, size);
   } else {
      int i;
      assignList(size, &i);
      Mem_free(ptr, i);
   }
}


// Realloc override
void * realloc(void *ptr, size_t size){

   int oldsize = ( ((chunk_t*)ptr) - 1)->size * sizeof(chunk_t);
   oldsize -= sizeof(chunk_t);
   if(oldsize <= PAGESIZE/4) free(ptr); // do first if not a seperate page

   void * temp = malloc(size);
   if(temp == NULL) return ptr;

   int nitems = size;
   if(oldsize < size){
      nitems = oldsize;
   }
   memmove(temp, ptr, nitems);

   if(oldsize > PAGESIZE/4) free(ptr); // unmap the old page
   return temp;  
}


/* function to request 1 or more pages from the operating system.
 *
 * new_bytes must be the number of bytes that are being requested from
 *           the OS with the sbrk command.  It must be an integer 
 *           multiple of the PAGESIZE
 *
 * returns a pointer to the new memory location.  If the request for
 * new memory fails this function simply returns NULL, and assumes some
 * calling function will handle the error condition.  Since the error
 * condition is catastrophic, nothing can be done but to terminate 
 * the program.
 */
chunk_t *morecore(int new_bytes) 
{

    void *new_p;
    int fd = open ("/dev/zero", O_RDWR);

    new_p = mmap(NULL, new_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE , fd, 0);
    if(new_p == NULL) return NULL;

    // update sbrk_stats - Ignore this its for debugging sbrk is NOT called
    sbrkCalls++;
    numPages += new_bytes/PAGESIZE;

    close(fd);
    return (chunk_t*)new_p;
}


/* Recursive function to start at the chunk_t* specified
*  and attempt to combine adjacent memory blocks until Dummy is found
* Preconditions: m is not NULL
*                if m is not pointing to Dummy, then m has size > 0
*                Rover is initialized and points to an item in the free list
*                
* Postcondition: Memory heap will be as defragmented as possible and blocks
*                will be as large as possible.
*/                
void mem_coalesce(chunk_t* m, int i){
   if(m == &Dummy_List[i]) return;   
   
   chunk_t* Rover = Rover_List[i];
   chunk_t* n = m->next;
   if(m + m->size == n){
      m->size += n->size;
      m->next = n->next;
      if(Rover == n) Rover = m->next;
      mem_coalesce(m, i);
   } 
   else
      mem_coalesce(n, i);

}

    
/* deallocates the space pointed to by return_ptr; it does nothing if
 * return_ptr is NULL.  
 *
 * This function assumes that the Rover pointer has already been 
 * initialized and points to some memory block in the free list.
 * 
 * Also used by Mem_alloc after call to morecore to apphend freelist with 
 * new page(s), since this block of memory is part of the free list at that point
 * This call is allowed and will function correctly.
 */
void Mem_free(void *return_ptr, int i)
{
    chunk_t *Rover = Rover_List[i];
    chunk_t Dummy = Dummy_List[i];

    // precondition
    assert(Rover != NULL && Rover->next != NULL);
    
    if(return_ptr == NULL) return; // do nothing
     
    chunk_t* new_ptr = (chunk_t*) return_ptr;
    new_ptr--;

    assert(new_ptr->size != 0);
    
    if(Coalescing == TRUE){
       chunk_t* curr, *prev;
       curr = Dummy.next;
       prev = &Dummy_List[i];
       
       while(curr != &Dummy_List[i] && new_ptr + new_ptr->size > curr){
          prev = curr;
          curr = curr->next;
       }

       prev->next = new_ptr;
       new_ptr->next = curr;
       Rover = Rover->next;
       mem_coalesce(Dummy.next, i);
    } else{                        // not coalescing
       new_ptr->next = Rover->next;
       Rover->next = new_ptr;
       Rover = Rover->next;
    }

}


/* returns a pointer to space for an object of size nbytes, or NULL if the
 * request cannot be satisfied.  The memory is uninitialized.
 *
 * This function assumes that there is a Rover pointer that points to
 * some item in the free list.  The first time the function is called,
 * Rover is null, and must be initialized with a dummy block whose size
 * is one, but set the size field to zero so this block can never be 
 * removed from the list.  After the first call, the Rover can never be null
 * again.
 */
void *Mem_alloc(const int nbytes, int i)
{

    chunk_t* Rover = Rover_List[i];

    // precondition
    assert(nbytes > 0);
    assert(Rover != NULL && Rover->next != NULL);

    int nunits = (nbytes / sizeof(chunk_t)) + 1; // plus 1 for header
    if( nbytes % sizeof(chunk_t) != 0) nunits++; // need enough bytes to compensate
                                                // if nbytes is not divisible by 
                                                // size of chunk_t
   
    chunk_t* curr = Rover;
    chunk_t* prev = &Dummy_List[i];
    for(; prev->next != curr; prev = prev->next);
 
    chunk_t* ret = NULL;
    
    // start search
    do{
       if(curr->size == nunits){
          prev->next = curr->next;
          ret = curr;
          break;
       } else if(curr->size > nunits){
          if(SearchPolicy == FIRST_FIT){     // first fit
             ret = curr;
             break;
          } else{
             if(SearchPolicy == BEST_FIT){   // best fit
                if(ret == NULL || curr->size < ret->size)
                   ret = curr;
             } else{                         // worst-fit
                if(ret == NULL || curr->size > ret->size)
                   ret = curr;
             }
          }             // end searchPolicy check
       }                // end else-if
       prev = curr;
       curr = curr->next;
    } while(curr != Rover);

    if(ret == NULL){             // need more pages
       int blockSize = (1<<i)/sizeof(chunk_t) + 1;
       int newPageSize = PAGESIZE / sizeof(chunk_t);
       newPageSize += blockSize - (newPageSize % blockSize);
       newPageSize *= sizeof(chunk_t);
       ret = morecore(newPageSize);

       //int new_pages = nbytes / PAGESIZE;
       //if( nbytes % PAGESIZE != 0) new_pages++; // need at least nunits
       //ret = morecore(new_pages * PAGESIZE);    
  
       if(ret == NULL){ 
          return NULL; // failed attempt
       } else{

          //ret->size = (new_pages * PAGESIZE)/sizeof(chunk_t);

          ret->size = newPageSize/sizeof(chunk_t);
          if(ret->size != nunits){             // not perfect match
             if(Coalescing == TRUE){
                Mem_free(ret+1, i); // "adds" new memory to free list in correct order
                                 // This is not a mistake, this works.
             } else{
                ret->next = Rover->next;
                Rover->next = ret;
             }
             
          } else{
             ret->next = NULL; // perfect match ready to return
          }               
       } // end else
    } // end if coalescing

    // prepare ret for return if a perfect match wasn't found
    if(ret->size != nunits){
       ret->size -= nunits;
       ret = ret + ret->size; // move to second half         
    }
    
    Rover = Rover->next; // this is done 1st in case ret == Rover 
    ret->next = NULL;
    ret->size = nunits;
    void* ret_final = (void*)(ret + 1);

    // postconditions 
    assert(ret + 1 == ret_final);
    assert((ret->size - 1)*sizeof(chunk_t) >= nbytes);
    assert((ret->size - 1)*sizeof(chunk_t) < nbytes + sizeof(chunk_t));
    assert(ret->next == NULL);  // saftey first!

    return ret_final;
}


/* prints stats about the current free list - THIS FUNCTION IS FOR DEBUG ONLY
 *
 * -- number of items in the linked list including dummy item
 * -- min, max, and average size of each item (in bytes)
 * -- total memory in list (in bytes)
 * -- number of calls to sbrk and number of pages requested
 *
 * A message is printed if all the memory is in the free list
 */
void Mem_stats(int i)
{
    //chunk_t *Rover = Rover_List[i];
    chunk_t Dummy = Dummy_List[i];

    int numItems = 0, minSize = 0, maxSize = 0, totalSize = 0, M = 0, T = 0;
    //assert(Rover != NULL && Rover->next != NULL);
    chunk_t* curr = Dummy.next;

    if(curr != &Dummy_List[i]){
       minSize = curr->size;
       maxSize = curr->size;
    }
    while(curr != &Dummy_List[i]){
       numItems++;
       if(curr->size < minSize) minSize = curr->size;
       if(curr->size > maxSize) maxSize = curr->size;
       totalSize += curr->size;
       curr = curr->next;
    }      
    M = totalSize*sizeof(chunk_t);
    T = numPages*PAGESIZE;
    int avgSize = numItems!=0?(totalSize/numItems)*sizeof(chunk_t):totalSize;
    printf("\n\n------------------\nMemory Stats\n------------------\n");
    printf("Total number of items in heap: %d\n", numItems);
    printf("Memory in Heap: %d Bytes\n", M);
    printf("Min block size: %ld Bytes -- Max block size: %ld Bytes\n", 
           minSize*sizeof(chunk_t), maxSize*sizeof(chunk_t));
    printf("Average block size: %d Bytes\n", avgSize);
    printf("Number of calls to sbrk(): %d\n", sbrkCalls);
    printf("Number of pages: %d\n", numPages);
    printf("Total memory in Heap: %d Bytes\n", T);
    if( M == T) 
       printf("all memory is in the heap -- no leaks are possible\n");
    else
       printf("not all memory has been returned to the heap -- leaks possible!\n");

    printf("-----------------\n End Stats\n-----------------\n\n");
}

/* print table of memory in free list - THIS FUNCTION IS FOR DEBUG ONLY
 *
 * The print should include the dummy item in the list 
 */
void Mem_print(int i)
{
   
    chunk_t *Rover = Rover_List[i];
    chunk_t *Dummy = &Dummy_List[i];
   
    assert(Rover != NULL && Rover->next != NULL);
    chunk_t *p = Dummy;
    do {
        // example format.  Modify for your design
        printf("p=%p, size=%d, end=%p, next=%p %s %s\n", 
                p, p->size, p + p->size,p->next, p->size!=0?"":"<-- dummy",
                p!=Rover?"":"<--Rover");
        p = p->next;
    } while (p != Dummy);
}

/* vi:set ts=8 sts=4 sw=4 et: */
