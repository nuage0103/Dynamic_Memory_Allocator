/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20221609",
    /* Your full name*/
    "ChoongIn Lee",
    /* Your email address */
    "hooin0318@sogang.ac.kr",
};

/*
You are not allowed to define any global or static compound data structures 
such as arrays, structs, trees, or lists in your mm.c program. 
However, you are allowed to declare global scalar variables such as
integers, floats, and pointers in mm.c.
*/

/* Basic constants and macros */
#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double word size (bytes) */
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes), 4096byte */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc)) // returns value that can be stored in header&footer

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7) // calculate allocated size
#define GET_ALLOC(p) (GET(p) & 0x1) // check if allocated(1) / free(0)

/* Given block ptr bp, compute address of its header and footer */
// bp points to the first payload byte, not header(start of the block)
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

static void *extend_heap(size_t words);
static char *extend_heap_realloc(char *bp, size_t asize, int *left);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void *place(void *bp, size_t asize);

// explicit free list
#define NEXT_FREE(bp) (*(char **)(bp))
#define PREV_FREE(bp) (*(char **)(bp + WSIZE))

static void insert(size_t size, char *new);
static void delete(char *del);

static void *heap_listp; // start of heap
static void *free_listp; // first free block

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    free_listp = NULL;
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) return -1;

    PUT(heap_listp, 0); /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); /* Epilogue header, end of heap memory space, used to avoid inapproriate access */
    heap_listp += (2*WSIZE);
    
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    //extend heap by 1024word
    if (extend_heap(1 << 6) == NULL) return -1;
    return 0;

}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    //printf("%d malloc\n", size);
    size_t asize; /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious requests */
    if (size == 0) return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE) asize = 2*DSIZE;
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        bp = place(bp, asize);
        return bp;
    }
    
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;
    bp = place(bp, asize);
    //printf("mallocEnd\n");
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    insert(size, bp);
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if(size == 0) {
        return NULL;
    }

    size_t asize;
    char *next = HDRP(NEXT_BLKP(ptr));
    size_t current_size = GET_SIZE(HDRP(ptr));
    size_t newsize = current_size + GET_SIZE(next);
        

    if (size <= DSIZE) asize = 2*DSIZE +(1 << 8);
    else asize = ALIGN(size + DSIZE) + (1 << 8);
    
    if(current_size >= asize) {
        return ptr;
    }
    if(!GET_ALLOC(next) && newsize >= asize ) {
        int left = newsize - asize;
        ptr = extend_heap_realloc(ptr, asize, &left);
        if(ptr == NULL) return NULL;
        delete(NEXT_BLKP(ptr));
        PUT(HDRP(ptr), PACK(asize + left, 1));
        PUT(FTRP(ptr), PACK(asize + left, 1));
        return ptr;
    }

    void *newptr = mm_malloc(size);
    if(newptr == NULL) return NULL;
    size_t oldSize = GET_SIZE(HDRP(ptr));
    size_t copySize = (oldSize < size)? oldSize : size;
    memcpy(newptr, ptr, copySize);
    mm_free(ptr);
    return newptr;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;; // odd : even
    if ((long)(bp = mem_sbrk(size)) == -1) return NULL;
    
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
    PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    insert(size, bp);
    
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

static char *extend_heap_realloc(char *bp, size_t asize, int *left)
{
    //printf("if\n");
    if(left && *left < 0){
        int extendsize = MAX(CHUNKSIZE, -(*left));
        if ((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;
        *left += extendsize;
    }
    //printf("ifend\n");
    return bp;
}


static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) { /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        delete(bp);
        delete(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
        insert(size, bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        delete(bp);
        delete(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        insert(size, bp);
    }

    else { /* Case 4 */
        delete(bp);
        delete(PREV_BLKP(bp));
        delete(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        insert(size, bp);
    }
    return bp;
}

static void *find_fit(size_t asize)
{   /* First-fit search */

    //printf("%d : findFit\n", asize);
    void *bp = free_listp;

    while(bp && (GET_SIZE(HDRP(bp)) < asize)) bp = NEXT_FREE(bp);
    //printf("%d : findFitEnd\n", asize);
    return bp;
}

static void *place(void *bp, size_t asize) {
    delete(bp);

    size_t csize = GET_SIZE(HDRP(bp));
    size_t free_left = csize - asize;

    if ((2*DSIZE) >= free_left) {
        asize = csize; // can't be split, allocate entire block
    } else if (asize < (ALIGNMENT << 3)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(free_left, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(free_left, 0));
        insert(free_left, NEXT_BLKP(bp));
    } else {
        // move to next block
        PUT(HDRP(bp), PACK(free_left, 0));
        PUT(FTRP(bp), PACK(free_left, 0));
        insert(free_left, bp);
        bp = NEXT_BLKP(bp);
    }

    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    return bp;
}

static void insert(size_t size, char *new)
{
    //printf("insert\n");
    //find free block, insert
    char *cur = free_listp;
    char *prev = NULL;

    while(cur && (GET_SIZE(HDRP(cur))) < size) {
        prev = cur;
        cur = NEXT_FREE(cur);
    }

    // start of free list
    if(prev == NULL){
        free_listp = new;
        PREV_FREE(new) = NULL;
        if(cur){
            NEXT_FREE(new) = cur;
            PREV_FREE(cur) = new;
        }
        else NEXT_FREE(new) = NULL; // only one element in list
    }
    else{
        PREV_FREE(new) = prev;
        NEXT_FREE(prev) = new;
        if(cur){
            NEXT_FREE(new) = cur;
            PREV_FREE(cur) = new;
        }
        else NEXT_FREE(new) = NULL; // new : last element
    }
    //printf("insert\n");
}

static void delete(char *del)
{
    //printf("delete\n");
    if(PREV_FREE(del) == NULL){
        free_listp = NEXT_FREE(del);
        if(free_listp) PREV_FREE(free_listp) = NULL;
    }
    else {
        NEXT_FREE(PREV_FREE(del)) = NEXT_FREE(del);
        if(NEXT_FREE(del)) PREV_FREE(NEXT_FREE(del)) = PREV_FREE(del);        
    }
    //printf("delete\n");
}