#ifndef JY_RANK_CACHE_H
#define JY_RANK_CACHE_H

#include "uthash.h"

#define CACHE_HIT 1
#define CACHE_MISS 0
#define COLDMISS -1234
/*
 *this file defines abstraction for rank cache
 *user will have to implement the priority mechanism for it to work properly.
 */

//TODO:
//		handle various size 
		//handle eps

//      handle cache size smaller than sample size in evictitem
//      handle random init
// struct _priority_t; //this struct will be used to keep all necessary priority info
// 					//user will have to manually define this struct

// typedef struct _priority_t priority_t;


struct _RankCache_t;

//Object structs
typedef struct _RankCache_Item_t {
	uint64_t key; //item's key
	uint64_t index; //this is the index of item in the cache, used for fast insertion
					//and fast sampling
	//item size user define, for logical size use 1.
	double size; 

	void* priority; // this is the struct for priority


	UT_hash_handle key_hh; //hash handler for fast key lookup
	UT_hash_handle pos_hh; //position handler for fast cache insertion
} RankCache_Item_t;


typedef struct  _RC_Stats_t {
	uint64_t totRef; //tot references to cache
	uint64_t totKey; //tot Key in cache
	uint64_t totMiss;
	uint64_t totGet; 
	uint64_t totSet;
	uint64_t totGetSet;
	uint64_t totDel; //record total number of delete operations
	uint64_t totEvict;
} RC_Stats_t;

typedef void* (*CreatePriority)(struct _RankCache_t*, RankCache_Item_t*);
typedef void (*UpdatePriorityOnHit)(struct _RankCache_t*, RankCache_Item_t*);
typedef void (*UpdatePriorityOnEvict)(struct _RankCache_t*, RankCache_Item_t*);
typedef RankCache_Item_t* (*MinPriorityItem)(struct _RankCache_t*, RankCache_Item_t*, RankCache_Item_t*);

typedef struct _RankCache_t {

	uint64_t clock; //logical clock // similar to totRef here

	uint32_t sample_size;

	/* current max index in the hash table
	 * total number of key in the hashtable
	 * depending on the size of item in the cache, currNum could go up and down
	 */
	uint64_t currNum; 

	//since item size is define by user
	//capacity and size is double to adapt to fraction number
	double currSize;
	double capacity; //cap reduce by item's size per access
					//the default size is 1, act as logical cache
	
	void* globalData; //pointer to struct holding global information for replacement policy

	CreatePriority createPriority;
	UpdatePriorityOnHit updatePriorityOnHit;
	UpdatePriorityOnEvict updatePriorityOnEvict;
	MinPriorityItem minPriorityItem;

	RC_Stats_t* stat;

	RankCache_Item_t *ItemIndex_HashTable; //fast item insert and random sampling
	RankCache_Item_t *Item_HashTable; //hashtable used for storing
} RankCache_t;




//check better naming

RankCache_t* cacheInit(double cap, 
					   uint32_t sam, 
					   CreatePriority pInit,
					   UpdatePriorityOnHit pUpdateOnHit,
					   UpdatePriorityOnEvict pUpdateOnEvict,
					   MinPriorityItem pMin
					   );
void cacheFree(RankCache_t* cache);



/******************************
 * methods for trunc the cache in runtime
 * this is probably needed for adap
 *
*******************************/
void cacheTrunc();
/*********************************/

//cache operations

//find object in the cache
// if exist return the object 
//if does not exist return NULL
RankCache_Item_t* RC_get(RankCache_t* cache, uint64_t key);

//set item, return 1 on completion else return 0
uint8_t RC_set(RankCache_t* cache, uint64_t key, uint64_t size);
//delete object,
//return deleted object on success else NULL
RankCache_Item_t* RC_del(RankCache_t* cache, uint64_t key);

//expired ops, TODO
uint8_t RC_delay_del(RankCache_t* cache, uint64_t key);

//standard caching operation
//return 1 on hits
//return 0 on miss
uint8_t RC_getAndSet(RankCache_t* cache, uint64_t key, uint64_t size);




uint64_t evictItem(RankCache_t* cache);
void addItem(RankCache_t* cache, RankCache_Item_t* item);

//for a logical cache, size is just one
RankCache_Item_t* createItem(uint64_t key, uint64_t size, uint64_t index); 
RankCache_Item_t* findItem(RankCache_t* cache, uint64_t key);




//iternal wrapper
void random_init();
uint64_t random_index();






#endif 