#include <stdlib.h> 
#include <stdio.h>
#include <time.h> 
#include <assert.h>
#include "uthash.h"
#include "murmur3.h"
#include "pcg_variants.h"
#include "entropy.h"
#include <string.h>
#include "RankCache.h"



//the capacity can be in arbitrary unit, makesure item size agree with it
RankCache_t* cacheInit(double cap, 
					   uint32_t sam, 
					   CreatePriority pInit,
					   UpdatePriorityOnHit pUpdateOnHit,
					   UpdatePriorityOnEvict pUpdateOnEvict,
					   MinPriorityItem pMin
					   ){

	RankCache_t* RC = malloc(sizeof(RankCache_t));
	if (RC == NULL) {
		perror("rankCache init failed!\n");
		return RC;
	} 
	uint64_t seeds[2];
    entropy_getbytes((void*)seeds, sizeof(seeds));
    pcg64_srandom(seeds[0], seeds[1]);
	RC->currNum = 0;
	RC->capacity = cap;
	RC->currSize = 0;
	RC->sample_size = sam;
	RC->clock = 0;

	RC->Item_HashTable = NULL;
	RC->ItemIndex_HashTable = NULL;

	RC->createPriority = pInit;
	RC->updatePriorityOnHit = pUpdateOnHit;
	RC->updatePriorityOnEvict = pUpdateOnEvict;
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


uint8_t access(RankCache_t* cache, uint64_t key, uint64_t size) {

	assert(cache != NULL);
	cache->clock++;
	cache->totRef++;

	RankCache_Item_t* item = findItem(cache, key);

	if (item == NULL) {//cache miss

		if (size > cache->capacity) {
			/*handle size is larger than cache,
			  for now we report error and terminate program,
			  second option is to skip it*/
			fprintf(stderr, "key: %ld size: %ld exceed cache capacity! \n", key, size);
			exit(-1);
		}

		uint64_t index = cache->currNum;
		/* update the cache size, if updated size is too large, evict some from curr cache */
		
		cache->currSize = cache->currSize + size;
		if(cache->currSize > cache->capacity) //eps handle
			index = evictItem(cache); //cache size reduce 

		item = createItem(key, size, index);
		item->priority = cache->createPriority(cache, item);
		addItem(cache, item); //cache currNum (index) increment++
		return CACHE_MISS;
	} else {
		cache->updatePriorityOnHit(cache, item); //increment hit &lifetime table
		return CACHE_HIT;
	}
}

//return index of evicted item
/*
 * This function will free enough space for newly entered item.
 * 
 * Logic:
 *		0. first we check, whether the item will be able to fit into the cache
 *		   if the item is too large. report error, because in most of the case
 *         this is not intended behavior.
 *			(this been checked before enter eviction)
 *
 *		1. we randomly select one element from cache
 *		   then check the cache size is enough for new item
 *		   if yes, return the index of evicted item
 *		   if no, place the last item in the evicted index
 *				  decrement the currNum
 *				  continue eviction
 *		2. cache->currNum = currNum - array size. 
 */

uint64_t evictItem(RankCache_t* cache) {

	assert(cache != NULL);

	RankCache_Item_t *temp1, *temp2, *min;
	uint64_t rand_index;
	uint64_t ret_index;

	
	while (1) { //eps needed
		rand_index = pcg64_random()%(cache->currNum); //randomly select one element
		temp1=NULL, temp2=NULL, min=NULL;
		HASH_FIND(pos_hh, cache->ItemIndex_HashTable, &rand_index, sizeof(uint64_t), temp1);
		assert(temp1 != NULL);
		//sampling
		min = temp1;
		for (int i = 1; i<cache->sample_size; i++) {
			rand_index = pcg64_random()%(cache->currNum);
			HASH_FIND(pos_hh, cache->ItemIndex_HashTable, &rand_index, sizeof(uint64_t), temp2);
			min = cache->minPriorityItem(cache, temp1, temp2);
			temp1 = min;
			temp2 = NULL;	
		}

		cache->updatePriorityOnEvict(cache, min);
		ret_index = min->index;
		HASH_DELETE(key_hh, cache->Item_HashTable, min);
		HASH_DELETE(pos_hh, cache->ItemIndex_HashTable, min);
		free(min->priority);
		free(min);
		cache->currNum--;
		cache->currSize -= min->size;
		cache->totKey--;
		if (cache->currSize > cache->capacity) { //eps need
			//currNum always pointed to next index, if one evicted
			//we should change the item with currNum index to smaller one
			//leave currNum empty
			if (ret_index != cache->currNum) {
				temp2= NULL;
				HASH_FIND(pos_hh, cache->ItemIndex_HashTable, &(cache->currNum), sizeof(uint64_t), temp2);
				HASH_DELETE(pos_hh, cache->ItemIndex_HashTable, temp2);
				temp2->index = ret_index;
				HASH_ADD(pos_hh, cache->ItemIndex_HashTable, index, sizeof(uint64_t), temp2);
			}
		}else {
			break;
		}

	}


	//evict
	return ret_index;
}

void addItem(RankCache_t* cache, RankCache_Item_t* item) {
	assert(item != NULL && cache != NULL);
	HASH_ADD(pos_hh, cache->ItemIndex_HashTable, index, sizeof(uint64_t), item);
	HASH_ADD(key_hh, cache->Item_HashTable, key, sizeof(uint64_t), item);
	cache->totKey++;
	cache->currNum++;
}

//for a logical cache, size is just one
RankCache_Item_t* createItem(
				uint64_t key, 
				uint64_t size, 
				uint64_t index) {

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




