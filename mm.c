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
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Team black cow",
    /* First member's full name */
    "smart-cau",
    /* First member's email address */
    "1995hyunwoo@naver.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE       4
#define DSIZE       8
#define CHUNKSIZE  (1<<12)
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)           (*(unsigned int*)(p))
#define PUT(p, val)      (*(unsigned int*)(p) = (val))

/* Read and write a word at address p */
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)    ((char*)(bp) - WSIZE) // header & footer의 size에는 header + payload + footer를 다 합한 8의 배수의 크기가 들어감
#define FTRP(bp)    ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)    ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE)))
#define PREV_BLKP(bp)    ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

/* declare variables & functions */
typedef enum _block_type { FREE, ALLOC } block_type;
static char* heap_listp;
static char* nextfit_lastp;
static void* extend_heap(size_t words);
static void* coalesce(void* bp);
static void place(char* bp, size_t asize);
static void* find_fit(size_t asize);
static void* first_fit(size_t asize);
static void* next_fit(size_t asize);
static size_t get_proper_size(size_t size);
static void* set_block(void* bp, size_t size, block_type type);
static int is_block_has_enough_size(void* bp, size_t size);


/* 
 * mm_init - initialize the malloc package.
    perform any necessary initializations, such as allocating the initial heap area.
    The return value should be -1 if there was a problem in performing the initialization, 0 otherwise.
 */
int mm_init(void)
{
    heap_listp = mem_sbrk(4 * WSIZE);
    
    if (heap_listp == (void*)-1)
        return -1;

    PUT(heap_listp, 0); // Aignment padding
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // prologue header
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // prologue footer
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1)); // Epilogue header
    heap_listp += (2 * WSIZE);
    nextfit_lastp = heap_listp;
    // extend the empty heap
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 * The mm malloc routine returns a pointer to an allocated block payload of at least size bytes. 
 * The entire allocated block should lie within the heap region and should not overlap with any other allocated chunk.
We will comparing your implementation to the version of malloc supplied in the standard C library (libc). 
Since the libc malloc always returns payload pointers that are aligned to 8 bytes, your malloc implementation should do likewise and always return 8-byte aligned pointers
 */
void *mm_malloc(size_t size)
{
    size_t asize = get_proper_size(size); // block size에 맞게 조정될 size
    size_t extend_size; // heap 공간이 부족할 경우 heap을 늘릴 크기
    char* bp;

    if (size == 0)
        return NULL;

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extend_size = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extend_size / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 The mm free routine frees the block pointed to by ptr. It returns nothing. 
 This routine is only guaranteed to work when the passed pointer (ptr) was returned by an earlier call to mm malloc or mm realloc and has not yet been freed.
 */
void mm_free(void *ptr)
{
    // 예외처리 안해줘도 되나? e.g., malloc or realloc되지 않은 ptr이 왔을 때
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 * if ptr is NULL, the call is equivalent to mm_malloc(size)
 * it size is equal to zero, the call is equivalent to mm_free(ptr)
 * if ptr is not NULL :
    * 새로운 size 크기에 상관 없이 할당 공간을 조절하면 됨.
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    // 크기가 0이면 free와 동일
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    // ptr이 NULL이면 malloc과 동일
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    // 복사할 데이터의 크기 계산
    oldsize = GET_SIZE(HDRP(ptr));

    if (size == oldsize)
        return ptr;

    size_t newsize = get_proper_size(size);
    // 원래 사이즈보다 작은 사이즈를 재할당 하는 경우 최적화
    if (size < oldsize) {
        if (oldsize > newsize && (oldsize - newsize) >= 2 * DSIZE) {            
            set_block(ptr, newsize, ALLOC);
            set_block(NEXT_BLKP(ptr), oldsize - newsize, FREE);
            coalesce(NEXT_BLKP(ptr));
            return ptr;
        }
        oldsize = size;
    }
    // 원래 사이즈보다 큰 경우 최적화
    if (size > oldsize) {
        void* next_bp = NEXT_BLKP(ptr);
        void* prev_bp = PREV_BLKP(ptr);
        size_t added_size = newsize - oldsize;
        // case 1. next block has enough size
        // heap의 끝 부분에 도착한 경우
        if (GET_ALLOC(HDRP(next_bp)) &&
            GET_SIZE(HDRP(next_bp)) == 0) {
                if (extend_heap(added_size / WSIZE) == (void*)-1)
                    return;
                set_block(ptr, newsize, ALLOC);
                return ptr;
            }
        if (is_block_has_enough_size(next_bp, added_size)) {
            PUT(HDRP(ptr), PACK(newsize, 1));
            PUT(FTRP(ptr), PACK(newsize, 1));
            set_block(ptr, newsize, ALLOC);
            set_block(NEXT_BLKP(ptr), GET_SIZE(HDRP(next_bp)) - added_size, FREE);            
            return ptr;
        }
        // case 2. prev block has enough size
        else if (is_block_has_enough_size(prev_bp, added_size)) {
            newptr = prev_bp;
            size_t prev_size = GET_SIZE(HDRP(prev_bp));
            PUT(HDRP(newptr), PACK(newsize, 1));
            memmove(newptr, ptr, oldsize);
            PUT(FTRP(newptr), PACK(newsize, 1));
            set_block(NEXT_BLKP(newptr), prev_size - added_size, FREE);
            coalesce(NEXT_BLKP(newptr));
            return newptr;
        }
        // case 3. prev & next가 모두 free이고, 이를 모두 합하면 새로운 할당을 할 수 있을 때
        else if (!GET_ALLOC(HDRP(prev_bp)) &&
                 !GET_ALLOC(HDRP(next_bp)) &&
                 (GET_SIZE(HDRP(prev_bp)) + GET_SIZE(HDRP(next_bp)) > added_size) &&
                 ((GET_SIZE(HDRP(prev_bp)) + GET_SIZE(HDRP(next_bp))) - added_size >= 2 * DSIZE)
        ) {
            newptr = prev_bp;
            size_t total_size = GET_SIZE(HDRP(prev_bp)) + GET_SIZE(HDRP(next_bp)) + oldsize;
            PUT(HDRP(newptr), PACK(newsize, 1));
            memmove(newptr, ptr, oldsize);
            PUT(FTRP(newptr), PACK(newsize, 1));
            set_block(NEXT_BLKP(newptr), total_size - added_size, FREE);
            coalesce(NEXT_BLKP(newptr));
            return newptr;
        }
    }

    newptr = mm_malloc(size);

    // malloc 실패
    if (!newptr) {
        return NULL;
    }

    memcpy(newptr, ptr, oldsize);

    // 기존 메모리 해제
    mm_free(ptr);

    return newptr;
}

static void *extend_heap(size_t words)
{
    char* bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) // 여기는 더 간단한 표현으로 바꿀 수 있지 않을까? (long) casting 제거
        return NULL;

    /* Init free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0)); // free block header
    PUT(FTRP(bp), PACK(size, 0)); // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // epilogue header. 

    /* Coalesce if the prev block was free */
    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // case 1. prev_block and next_block are all allocated
    if (prev_alloc && next_alloc) {
        nextfit_lastp = bp;
        return bp;
    }
        

    //case 2. prev_block is allocated, next_block is free
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0)); // header에 있는 size가 새로 갱신되었기에, 이 값을 참조하면 자연스레 합쳐진 block의 끝으로 감
    }

    // case 3. prev_block is free, next_block is allocated
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0)); // FTRP가 header의 size로 값을 접근하기 때문에 header size를 먼저 변경하면 의도한대로 작동하지 않음
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    // case 4. prev_block and next_block are both free
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    nextfit_lastp = bp;
    return bp;
}

static void place(char *bp, size_t asize)
{
    size_t current_size = GET_SIZE(HDRP(bp));
    size_t left_size = current_size - asize;
    if (left_size < 2 * DSIZE) {
        PUT(HDRP(bp), PACK(current_size, 1)); // allocated header
        PUT(FTRP(bp), PACK(current_size, 1)); // allocated footer
        return;
    }
    
    // set asize in allocated block
    PUT(HDRP(bp), PACK(asize, 1)); // allocated header
    PUT(FTRP(bp), PACK(asize, 1)); // allocated footer
    // put new size in next block overheads
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(left_size, 0)); // next header
    PUT(FTRP(bp), PACK(left_size, 0)); // next footer
}

static void *find_fit(size_t asize)
{
    /* next_fit */
    return next_fit(asize);
}

void *first_fit(size_t asize)
{
    char* bp = NEXT_BLKP(heap_listp);

    while (GET_SIZE(HDRP(bp)) > 0) {
        if (GET_SIZE(HDRP(bp)) >= asize && !GET_ALLOC(HDRP(bp))) {
            return (void*)bp;
        }
        bp = NEXT_BLKP(bp);
    }

    return NULL;
}

void *next_fit(size_t asize)
{
    char* bp = nextfit_lastp;
    while (GET_SIZE(HDRP(bp)) > 0)
    {
        if (GET_SIZE(HDRP(bp)) >= asize && !GET_ALLOC(HDRP(bp))) {
            return bp;
        }
        bp = NEXT_BLKP(bp);
    }
    bp = NEXT_BLKP(heap_listp);
    while (GET_SIZE(HDRP(bp)) > 0 && bp < nextfit_lastp)
    {
        if (GET_SIZE(HDRP(bp)) >= asize && !GET_ALLOC(HDRP(bp))) {
            return bp;
        }
        bp = NEXT_BLKP(bp);
    }
    return NULL;
}

size_t get_proper_size(size_t size)
{
    if (size <= DSIZE)
        return 2 * DSIZE;
    return DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
}

void* set_block(void* bp, size_t size, block_type type) {
    PUT(HDRP(bp), PACK(size, type));
    PUT(FTRP(bp), PACK(size, type));
}

int is_block_has_enough_size(void *bp, size_t added_size)
{
    return !GET_ALLOC(HDRP(bp)) && 
            (GET_SIZE(HDRP(bp)) > added_size) && 
            ((GET_SIZE(HDRP(bp)) - added_size) >= 2 * DSIZE);
}
