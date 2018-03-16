#include <stdio.h>  // needed for size_t
#include <unistd.h> // needed for sbrk
#include <assert.h> // needed for asserts
#include "dmm.h"
#include <stdlib.h>
#include <sys/mman.h>

/* You can improve the below metadata structure using the concepts from Bryant
 * and OHallaron book (chapter 9).
 */

typedef struct metadata {
  /* size_t is the return type of the sizeof operator. Since the size of an
   * object depends on the architecture and its implementation, size_t is used
   * to represent the maximum size of any object in the particular
   * implementation. size contains the size of the data object or the number of
   * free bytes
   */
  size_t size;
  struct metadata* next;
  struct metadata* prev; 
} metadata_t;

/* freelist maintains all the blocks which are not in use; freelist is kept
 * sorted to improve coalescing efficiency.
 */

static metadata_t* freelist = NULL;
//Don't forget to consider tight ends.

void add_to_freelist(metadata_t* newblock, size_t block_size){
    if(newblock==NULL){
       return; 
    }
    
    if(freelist==NULL){
        newblock->size=block_size;
        newblock->next=NULL;
        newblock->prev=NULL;
        freelist=newblock;
    }
    
    metadata_t* loop_block=freelist;
    
    while(loop_block!=NULL){
        
        if((loop_block->next)==NULL && (loop_block<newblock)){
            //Insert at the end
            if(freelist==loop_block){
                newblock->next=NULL;
                newblock->prev=freelist;
                newblock->size=block_size;
                freelist->next=newblock;
                return;
            }
            newblock->next=NULL;
            newblock->prev=loop_block;
            newblock->size=block_size;
            loop_block->next=newblock;
            return;
            
            
        }
        else if((loop_block->next)==NULL && (loop_block>newblock)){
            if(freelist==loop_block){
               freelist=newblock;//new Head of freelist
                freelist->next=loop_block;
                freelist->prev=NULL;
                freelist->size=block_size;
                loop_block->prev=freelist;
                loop_block->next=NULL;
                return;
            }
            newblock->next=loop_block;
            newblock->prev=loop_block->prev;
            newblock->size=block_size;
            loop_block->prev=newblock;
            loop_block->next=NULL;
            return; 
            
        }else if(loop_block<newblock && (loop_block->next)>newblock){
            newblock->next=loop_block->next;
            newblock->prev=loop_block;
            newblock->size=block_size;
            loop_block->next=newblock;
            (loop_block->next)->prev=newblock;
            if(freelist==loop_block){
                freelist->next=newblock;
            }
            return;   
        }
        
        loop_block=loop_block->next;
        
    }
     
}
    


void remove_from_freelist(metadata_t* curr){
    if(curr==NULL){
        return;
    }
    if(curr==freelist){//If we were removing the head.
        if (curr->next==NULL){
            //one item only in list.
        
        freelist=NULL;
        return; 
        }
        else{
           freelist=curr->next;
            (curr->next)->prev=NULL;
        }   
    }
    else{
        
    metadata_t* loop_curr=freelist;
    while(loop_curr!=NULL){
        
        if((loop_curr->next)==curr){
            loop_curr->next=curr->next;
            if(curr->next!=NULL){
                (curr->next)->prev=loop_curr;
                curr->next=NULL;
                curr->prev=NULL;
            }
            
            return;
        } 
        loop_curr=loop_curr->next;
    }  
}
}

void coalesce(){
    metadata_t* curr_block=freelist;
    if(freelist==NULL || (freelist->next==NULL)){
        //Nothing to coalesce!
        return;
    }
    while(curr_block!=NULL){
        size_t new_aligned=ALIGN(curr_block->size);
        metadata_t* temp= (metadata_t*)((void*)curr_block+ METADATA_T_ALIGNED+new_aligned);
        metadata_t* currNext=curr_block->next;
        if(currNext!=NULL && temp==currNext){//If blocks are adjacent
            
    curr_block->size=(curr_block->size)+
    currNext->size+METADATA_T_ALIGNED;//Combine them
            
    curr_block->next=currNext->next;
            metadata_t* currNextNext=currNext->next;
            if(currNextNext!=NULL){
                currNextNext->prev=curr_block;
            }
        
         return;
        }
        curr_block=curr_block->next;
    }
     
}

void* dmalloc(size_t numbytes) {
  /* initialize through sbrk call first time */
  if(freelist == NULL) { 			
    if(!dmalloc_init())
      return NULL;
  }

  assert(numbytes > 0);
    
  /* your code here */ 
    //Find Free block.
    metadata_t* curr = freelist;
    metadata_t* to_ret=NULL;
   
    size_t aligned_bytes=ALIGN(numbytes);//bytes to find.
    
    while(curr!=NULL){
        if((curr->size)>=aligned_bytes){
            //printf(" Aligned bytes :%zd",aligned_bytes);
            //printf("\n");
            
            //printf("Curr->Size: %zd",curr->size);
            //printf("\n");
            break;
        }
        curr=curr->next;   
    }
    
    /*Return Null if we can't find a free block*/
    if(curr==NULL){
        return NULL;
    }
    
    
    /*Split(if necessary) and return pointer*/
    if(curr->size==aligned_bytes){//No need for splitting
         to_ret=(metadata_t*)((void*)curr + METADATA_T_ALIGNED);
         remove_from_freelist(curr);
         return (void*) to_ret;  
    }
    else{//Split, return pointer and adjust freeList!
        //printf("Pointer is curr %p ", curr);
        to_ret=(metadata_t*)((void*)curr + METADATA_T_ALIGNED);
        //printf("Pointer is to_ret %p ", to_ret);
        
    size_t free_size=(curr->size)-aligned_bytes;
        //printf("Free_Size: %zd",free_size);
            //printf("\n");
        
        curr->size=aligned_bytes;
        //printf("Curr->Size Afterwards: %zd",curr->size);
           // printf("\n");
        
        if(free_size>METADATA_T_ALIGNED){
            free_size=free_size-METADATA_T_ALIGNED;
            //printf("New Free_Size: %zd", free_size);
             //printf("\n");
            metadata_t* newblock=(metadata_t*)((void*)to_ret + aligned_bytes);
           add_to_freelist(newblock,free_size); 
            
        }
        remove_from_freelist(curr);
        //print_freelist();
        return (void*) to_ret;
    }
    
  return NULL;
    
}

void dfree(void* ptr) {
   //your code here 
    ptr=(void*)ptr-METADATA_T_ALIGNED;
    metadata_t* freed_block =(metadata_t*)ptr;
    size_t b_size=freed_block->size;
    add_to_freelist(freed_block,b_size);
    //printf("New Free_Size: %zd", free_size);
             //printf("\n");
    coalesce();   
}

bool dmalloc_init() {

  /* Two choices: 
   * 1. Append prologue and epilogue blocks to the start and the
   * end of the freelist 
   *
   * 2. Initialize freelist pointers to NULL
   *
   * Note: We provide the code for 2. Using 1 will help you to tackle the 
   * corner cases succinctly.
   */

  size_t max_bytes = ALIGN(MAX_HEAP_SIZE);
  /* returns heap_region, which is initialized to freelist */
  //freelist = (metadata_t*) sbrk(max_bytes); 
    
int prot = (PROT_WRITE | PROT_READ);
int flags = (MAP_SHARED | MAP_ANONYMOUS);    
    
freelist = (metadata_t*)mmap(NULL, max_bytes, prot, flags, -1, 0); 
  /* Q: Why casting is used? i.e., why (void*)-1? */
  if (freelist == (void *)-1)
    return false;
  freelist->next = NULL;
  freelist->prev = NULL;
  freelist->size = max_bytes-METADATA_T_ALIGNED;
  return true;
}

/* for debugging; can be turned off through -NDEBUG flag*/
void print_freelist() {
  metadata_t *freelist_head = freelist;
  while(freelist_head != NULL) {
    printf("\tFreelist Size:%zd, Head:%p, Prev:%p, Next:%p\t",
	  freelist_head->size,
	  freelist_head,
	  freelist_head->prev,
	  freelist_head->next);
    freelist_head = freelist_head->next;
  }
  printf("\n");
}
