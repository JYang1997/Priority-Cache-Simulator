#include <stdlib.h> 
#include <stdio.h>
#include <time.h> 
#include <assert.h>
#include "uthash.h"
#include "murmur3.h"
#include <string.h>
#include "RankCache.h"



//the capacity can be in arbitrary unit, makesure item size agree with it
RankCache_t* cacheInit(double cap, 
					   uint32_t sam, 
					   void* (*pInit)(RankCache_t*, RankCache_Item_t*),
					   void (*pUpdate)(RankCache_t*, RankCache_Item_t*),
					   RankCache_Item_t* (*pMin)(RankCache_t*, RankCache_Item_t*, RankCache_Item_t*)
					   ){

	RankCache_t* RC = malloc(sizeof(RankCache_t));
	if (RC == NULL) {
		perror("rankCache init failed!\n");
		return RC;
	} 
	srand(time(0));
	RC->capacity = cap;
	RC->currSize = 0;
	RC->sample_size = sam;
	RC->clock = 0;

	RC->Item_HashTable = NULL;
	RC->ItemIndex_HashTable = NULL;

	RC->createPriority = pInit;
	RC->updatePriority = pUpdate;
	RC->minPriorityItem = pMin;

	RC->totKey = 0;
	RC->totRef = 0;
	return RC;
}

void cacheFree(RankCache_t* cache) {

  RankCache_Item_t *currItem, *tmp;

  HASH_ITER(key_hh, cache->Item_HashTable, currItem, tmp) {
  	HASH_DELETE(pos_hh,cache->ItemIndex_HashTable, currItem);  /* delete; users advances to next */
    HASH_DELETE(key_hh,cache->Item_HashTable, currItem);
    free(currItem->priority);
    free(currItem);            /* optional- if you want to free  */
  }
}


uint8_t access(RankCache_t* cache, uint64_t key, uint32_t size) {

	assert(cache != NULL);
	cache->clock++;
	cache->totRef++;

	RankCache_Item_t* item = findItem(cache, key);

	if (item == NULL) {//cache miss
		cache->totKey++;

		uint32_t index = cache->currSize;
		if(cache->currSize == cache->capacity)	
			index = evictItem(cache); //cache size --

		item = createItem(key, size, index);
		item->priority = cache->createPriority(cache, item);
		addItem(cache, item); //cache size++
		return CACHE_MISS;
	} else {
		cache->updatePriority(cache, item);
		return CACHE_HIT;
	}
}

//return index of evicted item
uint32_t evictItem(RankCache_t* cache) {

	assert(cache != NULL);

	//change random later
	
	
	uint32_t rand_index = rand()%(cache->currSize);
	RankCache_Item_t *temp1=NULL, *temp2=NULL, *min=NULL;
	HASH_FIND(pos_hh, cache->ItemIndex_HashTable, &rand_index, sizeof(uint32_t), temp1);
	assert(temp1 != NULL);
	//sampling
	min = temp1;
	for (int i = 1; i<cache->sample_size; i++) {
		rand_index = rand()%(cache->currSize);
		HASH_FIND(pos_hh, cache->ItemIndex_HashTable, &rand_index, sizeof(uint32_t), temp2);
		min = cache->minPriorityItem(cache, temp1, temp2);
		temp1 = min;
		temp2 = NULL;	
	}


	//evict
	uint32_t index = min->index;
	HASH_DELETE(key_hh, cache->Item_HashTable, min);
	HASH_DELETE(pos_hh, cache->ItemIndex_HashTable, min);
	free(min->priority);
	free(min);

	cache->currSize--;
	
	return index;
}

void addItem(RankCache_t* cache, RankCache_Item_t* item) {
	assert(item != NULL && cache != NULL);
	HASH_ADD(pos_hh, cache->ItemIndex_HashTable, index, sizeof(uint32_t), item);
	HASH_ADD(key_hh, cache->Item_HashTable, key, sizeof(uint64_t), item);
	cache->currSize++;
}

//for a logical cache, size is just one
RankCache_Item_t* createItem(
				uint64_t key, 
				uint32_t size, 
				uint32_t index) {

	RankCache_Item_t *item = malloc(sizeof(RankCache_Item_t));
	if (item == NULL) {
		perror("item init failed!\n");
		return item;
	}
	item->key = key;
	item->index = index;
	item->size = size;
	item->priority = NULL;
	
	return item;
}


//internal helper
RankCache_Item_t* findItem(RankCache_t* cache, uint64_t key) {
	RankCache_Item_t* item = NULL;
	HASH_FIND(key_hh, cache->Item_HashTable, &key, sizeof(uint64_t), item);
	return item;
}




