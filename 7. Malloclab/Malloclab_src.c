#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"
#define ALIGNMENT 8
#define WSIZE 4 
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
// 교과서 Figure 9.43에 있는 Macro % Constant와 편의를 위한 Macro를 추가하였습니다.
//이번 과제에서 사용한 동적할당 기법은 Explicit Free List & First Fit입니다.
//이를 선택한 이유는 Report에서 밝히겠습니다.
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define CHUNKSIZE (1 << 12) //Heap Size를 늘립니다
#define MAX(x, y) ((x) > (y) ? (x) : (y)) 
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (* ((unsigned int *) (p)))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
#define GET_SIZE(p) ((unsigned int)GET(p) & ~(ALIGNMENT-1))
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp) ((void *)(bp) - WSIZE)
#define FTRP(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)) - 2*WSIZE)
#define NEXT_BLKP(bp) ((void*)(bp) + GET_SIZE(((void *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((void*)(bp) - GET_SIZE(((void *)(bp) - 2*WSIZE)))
// Pointer 연산의 편의를 위한 Macro
#define IGNORE(value) (assert((int)value || 1))
#define PREV_FREE_BLKP(ptr) (*(void **) (ptr))
#define NEXT_FREE_BLKP(ptr) (*(void **) (ptr + WSIZE))
#define SET_PREV_FREE(bp, prev) (*((void **)(bp)) = prev) 
#define SET_NEXT_FREE(bp, next) (*((void **)(bp + WSIZE)) = next) 

static void *free_list_head; /* Explicit Free List의 Head를 가리키는 Pointer.*/
static void *heap_bottom; /* Epilogue를 가리키는 Pointer.*/
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *place(void *bp, size_t asize);
static void *SearchForFit(size_t asize);
static void insert_node(void *bp);
static void delete_node(void *bp);
int mm_check(char *function);


int mm_init(void)
{
    
    // 비어있는 Heap을 만들기 위해 Initialization.
    if ((heap_bottom = mem_sbrk(4*WSIZE)) == (void *)-1) 
	{
        return -1;
    }
	//Setting!
    PUT(heap_bottom, 0); /* Alignment padding */
    PUT(heap_bottom + (1*WSIZE), PACK(ALIGNMENT, 1)); /* Prologue header */
    PUT(heap_bottom + (2*WSIZE), PACK(ALIGNMENT, 1)); /* Prologue footer */
    PUT(heap_bottom + (3*WSIZE), PACK(0, 1)); /* Epilogue header */
    
    heap_bottom += 2*WSIZE;
    
    free_list_head = NULL;
	/* 비어있는 Heap을 Chucksize로 늘리고 그 늘린 새로운 메모리의 블록 포인터를 Free List의 head로 설정합니다.
	*/
    if ((extend_heap(CHUNKSIZE/WSIZE)) == NULL) 
	{
        return -1;
    }
	/*#ifdef DEBUG
    mm_check(__FUNCTION__);
	#endif */
       return 0;
}
void *mm_malloc(size_t size)
{   size_t asize; 
    size_t extendsize; 
    void *bp;
    
    if (size == 0) 
	{
        return NULL;
    }
    
   //SIZE SETTING
    if (size <= ALIGNMENT) 
	{
        asize = 2 * ALIGNMENT; 
    } 
	else 
	{
        asize = ALIGN(size + (2 * WSIZE)); 
    }
    
    /* Size에 적합한 Free List를 찾습니다.*/
    if ((bp = SearchForFit(asize)) != NULL) 
	{
        bp = place(bp, asize);
		/* #ifdef DEBUG
    	mm_check(__FUNCTION__);
		#endif */
        return bp;
    }
    
    /* 만약 없다면 extend_heap을 통해 heap을 확장합니다. */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
	{
        return NULL; //더 이상의 Heap X
    }
    bp = place(bp, asize);
  	/* #ifdef DEBUG
    	mm_check(__FUNCTION__);
		#endif */
    return bp;
}
void mm_free(void *ptr)
{
 	size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);  
		/* #ifdef DEBUG
    	mm_check(__FUNCTION__);
		#endif */  
}


void *mm_realloc(void *ptr, size_t size)
{  	size_t asize; 
    void *bp;
    if (size == 0) 
	{
        return NULL;
    }
    if (size <= ALIGNMENT) 
	{
        asize = 2 * ALIGNMENT; 
    }
	else 
	{
        asize = ALIGN(size + (2 * WSIZE));
    }
    
    size_t currentSize = GET_SIZE(HDRP(ptr));
    
    if (currentSize > asize) 
	{
        bp = place(ptr, asize);
        assert(bp == ptr);
    } 
	else if (currentSize < asize) 
	{
        void *next_bp = NEXT_BLKP(ptr);
        void *nextHeader = HDRP(next_bp);
        /* 다음 블록에 새로 요구된 맞는 사이즈가 있는지 체크 */
      
        if (!GET_ALLOC(nextHeader) && GET_SIZE(nextHeader) >= (asize-currentSize )) 
		{
            delete_node(next_bp);
            PUT(HDRP(ptr), PACK(currentSize + GET_SIZE(nextHeader), 0));
            PUT(FTRP(ptr), PACK(currentSize + GET_SIZE(nextHeader), 0));
            
            void *temp1; // for Previous Pointer
            void *temp2; // for Next Pointer
            memcpy(&temp1, ptr, WSIZE);
            memcpy(&temp2, ptr + WSIZE, WSIZE);
            insert_node(ptr);
            bp = place(ptr, asize);
            memcpy(bp, &temp1, WSIZE);
            memcpy(bp + WSIZE, &temp2, WSIZE);
        } 
		else if ((bp = SearchForFit(asize)) != NULL) //Free List 찾는 곳
		{  
            bp = place(bp, asize);
            memcpy(bp, ptr, currentSize - (2 * WSIZE));
            mm_free(ptr);
        } 
		else 
		{
            // 없으므로 새로운 힙영역 확장
            size_t extendsize = MAX(asize, CHUNKSIZE);
            if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
			{
                return NULL; 
            }
            bp = place(bp, asize);
            memcpy(bp, ptr, currentSize - (2 * WSIZE));
            mm_free(ptr);
        }
    } 
	else 
	{
        bp = ptr;
    }
  
    return bp;
}
static void insert_node(void *bp) 
{
    void *current = free_list_head;
    void *hold = current;
    void *prev = NULL;
    while (current != NULL && bp < current) 
	{
        prev = PREV_FREE_BLKP(current);
        hold = current;
        current = NEXT_FREE_BLKP(current);
    }
    
    SET_PREV_FREE(bp, prev);
    SET_NEXT_FREE(bp, hold);
    if (prev != NULL) 
	{
        SET_NEXT_FREE(prev, bp);
    } 
	else 
	{ 
        free_list_head = bp;
    }
    if (hold != NULL) 
	{
        SET_PREV_FREE(hold, bp);
    }
}
static void *place(void *bp, size_t asize) 
{
    size_t size = GET_SIZE(HDRP(bp));
	int initial_free_size = size-asize;
    if ((size - asize) >= (2*ALIGNMENT)) 
	{ 
        delete_node(bp);
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        void *initial = bp;
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(initial_free_size, 0));
        PUT(FTRP(bp), PACK(initial_free_size, 0));
        insert_node(bp);
        bp = initial;
    } //Split!
	else 
	{ 
        PUT(HDRP(bp), PACK(size, 1));
        PUT(FTRP(bp), PACK(size, 1));
        delete_node(bp);
    }
    return bp;
}
static void *SearchForFit(size_t asize) 
{
    void *bp;
    for(bp = free_list_head; bp != NULL; bp = NEXT_FREE_BLKP(bp)) 
	{
        if(asize <= GET_SIZE(HDRP(bp))) 
		{
            return bp;
        }
    }
    return NULL; 
}
static void *extend_heap(size_t words) 
{
    void *bp;
    size_t size;
    size = ALIGN(words * WSIZE);
    if ((long)(bp =  mem_sbrk(size)) == -1) 
	{ 
        return NULL;
    }//Error 처리
    /*Initialize & Setting For Basic Heap*/
    PUT(HDRP(bp), PACK(size, 0)); /* free block header  */
    PUT(FTRP(bp), PACK(size, 0)); /* free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */
    return coalesce(bp);/* 전 블록이 만약 free 상태였다면 coalesce가 필요하므로 coalesce를 call합니다. */
}
/* By LIFO Policy*/
 static void *coalesce(void *bp) 
 {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    if (prev_alloc && next_alloc) // Case 1 (PDF에 있는 Case들 입니다.)
	{
        insert_node(bp);
        return bp;
    }
    else if (prev_alloc && !next_alloc)// Case 2
    {
        delete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        insert_node(bp);
    }
    else if (!prev_alloc && next_alloc)// Case 3 
	{
        delete_node(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        insert_node(bp);
    }
    else // Case 4
	{
        delete_node(PREV_BLKP(bp));
        delete_node(NEXT_BLKP(bp));
        size+=GET_SIZE(HDRP(PREV_BLKP(bp)));
        size+=GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        insert_node(bp);
    }
    return bp;
}
static void delete_node(void *bp) 
{
    void *next = (void *) NEXT_FREE_BLKP(bp);
    void *prev = (void *) PREV_FREE_BLKP(bp);
    if (prev == NULL) 
	{ 
        free_list_head = next;
    } 
	else 
	{
        SET_NEXT_FREE(prev, next);
    }
    
    if (next != NULL) 
	{ 
        SET_PREV_FREE(next, prev);
    }
}
/***********************************************/
/*int mm_check(char *function)
{
    printf("---cur function:%s empty blocks:\n",function);
    char *tmpP = GET(free_list_head);
    int count_empty_block = 0;
    while(tmpP != NULL)
    {
        count_empty_block++;
        printf("address：%x size:%d \n",tmpP,GET_SIZE(HDRP(tmpP)));
        tmpP = GET(NEXT_LINKNODE_RP(tmpP));
    }
    printf("empty_block num: %d\n",count_empty_block);
}*/