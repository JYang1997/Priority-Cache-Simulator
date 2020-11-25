#ifndef JY_RANK_CACHE_H
#define JY_RANK_CACHE_H

#include "uthash.h"

#define CACHE_HIT 1
#define CACHE_MISS 0
/*
 *this file defines abstraction for rank cache
 *user will have to implement the priority mechanism for it to work properly.
 */

//TODO:
//		handle various size
//		with two hashtable
//      first select enough item for eviction
//      then memorize those items index
//      start from back, exchange back element with 

//      handle cache size smaller than sample size in evictitem
//      handle random init
// struct _priority_t; //this struct will be used to keep all necessary priority info
// 					//user will have to manually define this struct

// typedef struct _priority_t priority_t;



struct _RankCache_t;

//Object structs
typedef struct _RankCache_Item_t {
	uint64_t key; //item's key
	uint32_t index; //this is the index of item in the cache, used for fast insertion
					//and fast sampling
	//item size user define, for logical size use 1.
	double size; 

	void* priority; // this is the struct for priority


	UT_hash_handle key_hh; //hash handler for fast key lookup
	UT_hash_handle pos_hh; //position handler for fast cache insertion
} RankCache_Item_t;


typedef struct _RankCache_t {
	uint32_t totKey;
	uint32_t totRef; //not neccessary parameters

	uint32_t clock; //logical clock // similar to totRef here

	uint32_t sample_size;
	//since item size is define by user
	//capacity and size is double to adapt to fraction number
	uint32_t currSize;
	uint32_t capacity; //cap reduce by item's size per access
					//the default size is 1, act as logical cache
	
	void* (*createPriority)(struct _RankCache_t*, RankCache_Item_t*); //for now use fixed variables
	void (*updatePriority)(struct _RankCache_t*, RankCache_Item_t*);
	RankCache_Item_t* (*minPriorityItem)(struct _RankCache_t*, RankCache_Item_t*, RankCache_Item_t*);

	RankCache_Item_t *ItemIndex_HashTable; //fast item insert and random sampling
	RankCache_Item_t *Item_HashTable; //hashtable used for storing
} RankCache_t;




RankCache_t* cacheInit(double cap, 
					   uint32_t sam, 
					   void* (*pInit)(RankCache_t*, RankCache_Item_t*),
					   void (*pUpdate)(RankCache_t*, RankCache_Item_t*),
					   RankCache_Item_t* (*pMin)(RankCache_t*, RankCache_Item_t*, RankCache_Item_t*)
					   );
void cacheFree(RankCache_t* cache);



/******************************
 * methods for trunc the cache in runtime
 * this is probably needed for adapt to 
 *
*******************************/
void cacheTrunc();
/*********************************/




uint8_t access(RankCache_t* cache, uint64_t key, uint32_t size);
uint32_t evictItem(RankCache_t* cache);
void addItem(RankCache_t* cache, RankCache_Item_t* item);

//for a logical cache, size is just one
RankCache_Item_t* createItem(uint64_t key, uint32_t size, uint32_t index); 
RankCache_Item_t* findItem(RankCache_t* cache, uint64_t key);

//iternal wrapper
void random_init();
uint32_t random_index();






#endif /*JY_LFU_LIST_STACK_H*/