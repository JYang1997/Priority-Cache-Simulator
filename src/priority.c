#include "RankCache.h"
#include "hist.h"
#include "uthash.h"
#include "priority.h"
#include <assert.h>

/*********************************HC priority interface************************************************/

void* HC_initPriority(RankCache_t* cache, RankCache_Item_t* item) {

	HC_Priority_t* p = malloc(sizeof(HC_Priority_t));
	p->refCount = 0;
	p->enterTime = cache->clock;

	return p;
} 

void HC_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item) {

	HC_Priority_t* p = (HC_Priority_t*)(item->priority);
	p->refCount++;
	// p->lastAccessTime = cache->clock;

}


void HC_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item) {

}

RankCache_Item_t* HC_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2) {

	assert(item1 != NULL);
	assert(item2 != NULL);
	double p1, p2;
	HC_Priority_t* pp1 = (HC_Priority_t*)(item1->priority);
	HC_Priority_t* pp2 = (HC_Priority_t*)(item2->priority);
	p1 = (pp1->refCount) / (double)((cache->clock - pp1->enterTime)*(item1->size));
	p2 = (pp2->refCount) / (double)((cache->clock - pp2->enterTime)*(item2->size));

	return p1 <= p2 ? item1 : item2;
}



/*********************************LHD priority interface************************************************/



void LHD_globalDataInit(RankCache_t* cache) {
	LHD_globalData* gd = malloc(sizeof(LHD_globalData));

	gd->hitHist = histInit(128,1024*1024*10, 128);
	gd->lifeTimeHist = histInit(128,1024*1024*10, 128);
	gd->lhdHist = histInit(128,1024*1024*10, 128);

	gd->lastUpdateTime = 0;
	gd->lhd_period = 100000;

	cache->globalData = (void*)gd;
}

void* LHD_initPriority(RankCache_t* cache, RankCache_Item_t* item) {
	LHD_globalData* gd = (LHD_globalData*) cache->globalData;

	LHD_Priority_t* p = malloc(sizeof(LHD_Priority_t));
	p->lastAccessTime = cache->clock;
	addToHist(gd->hitHist, COLDMISS);
	addToHist(gd->lifeTimeHist, COLDMISS);
	return p;
} 

void LHD_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item) {
	
	LHD_globalData* gd = (LHD_globalData*) cache->globalData;

	LHD_Priority_t* p = (LHD_Priority_t*)(item->priority);

	uint64_t age = (cache->clock)-(p->lastAccessTime);
	addToHist(gd->hitHist, age);
	addToHist(gd->lifeTimeHist, age);
	
	p->lastAccessTime = cache->clock;
}


void LHD_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item) {
	
	LHD_globalData* gd = (LHD_globalData*) cache->globalData;

	LHD_Priority_t* p = (LHD_Priority_t*)(item->priority);
	uint64_t age = (cache->clock)-(p->lastAccessTime);
	addToHist(gd->lifeTimeHist, age);

}

void updateLHDHist(RankCache_t* cache) {
	
	
	LHD_globalData* gd = (LHD_globalData*) cache->globalData;

	assert(gd->lhdHist->size > 0);
	
	

	//include cold miss? or not
	double acc_Hit = gd->hitHist->Hist[gd->lhdHist->size-1];
	double acc_lifetime = gd->lifeTimeHist->Hist[gd->lhdHist->size-1];
	double exp_lifetime = acc_lifetime;

	gd->lhdHist->Hist[gd->lhdHist->size-1] = (acc_Hit/(gd->hitHist->tot))/(exp_lifetime/(gd->lifeTimeHist->tot));

	for (int i = gd->lhdHist->size-2; i >= 0 ; --i)
	{

		gd->lhdHist->Hist[i] = (acc_Hit/(gd->hitHist->tot))/(exp_lifetime/(gd->lifeTimeHist->tot));
		acc_Hit += gd->hitHist->Hist[i];
		acc_lifetime += gd->lifeTimeHist->Hist[i];
		exp_lifetime  += acc_lifetime;
	}
}

RankCache_Item_t* LHD_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2) {
	
	assert(item1 != NULL);
	assert(item2 != NULL);
	
	LHD_globalData* gd = (LHD_globalData*) cache->globalData;
	//check when is last time update the table
	//if 10000 reference passed, update it
	if (cache->clock - gd->lhd_period > gd->lastUpdateTime){
		gd->lastUpdateTime = cache->clock;
		updateLHDHist(cache);
	//	printf("%lld\n", cache->totRef);
	}

	double p1, p2;
	LHD_Priority_t* pp1 = (LHD_Priority_t*)(item1->priority);
	LHD_Priority_t* pp2 = (LHD_Priority_t*)(item2->priority);

	//evict the one with low priority
	uint64_t age1 = (cache->clock)-(pp1->lastAccessTime);
	uint64_t age2 = (cache->clock)-(pp2->lastAccessTime);

	int lhd_index1 = (((int)age1 - gd->lhdHist->first) / (gd->lhdHist->interval)) + 1;
	int lhd_index2 = (((int)age2 - gd->lhdHist->first) / (gd->lhdHist->interval)) + 1;

	if(lhd_index1 > gd->lhdHist->size-1) {
		lhd_index1 = gd->lhdHist->size-1;
	}

	if(lhd_index2 > gd->lhdHist->size-1) {
		lhd_index2 = gd->lhdHist->size-1;
	}

	//both mul hist step = not
	//p1 = gd->lhdHist->Hist[lhd_index1]/(histstep*(item1->size));
	//p2 = gd->lhdHist->Hist[lhd_index2]/(histstep*(item2->size));

	p1 = gd->lhdHist->Hist[lhd_index1]/(item1->size);
	p2 = gd->lhdHist->Hist[lhd_index2]/(item2->size);

	//printf("age1: %d p1 %f age2: %d p2 %f tot: %lld\n",age1, p1, age2, p2,  cache->totRef);
	return p1 <= p2 ? item1 : item2;
}


/*********************************LRU priority interface************************************************/

void* LRU_initPriority(RankCache_t* cache, RankCache_Item_t* item) {
	LRU_Priority_t* p = malloc(sizeof(LRU_Priority_t));
	p->lastAccessTime = cache->clock;
	return p;
} 

void LRU_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item) {
	LRU_Priority_t* p = (LRU_Priority_t*)(item->priority);
	p->lastAccessTime = cache->clock;
}


void LRU_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item) {
}



RankCache_Item_t* LRU_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2) {
	
	assert(item1 != NULL);
	assert(item2 != NULL);

	LRU_Priority_t* pp1 = (LRU_Priority_t*)(item1->priority);
	LRU_Priority_t* pp2 = (LRU_Priority_t*)(item2->priority);
	uint64_t p1 = pp1->lastAccessTime;
	uint64_t p2 = pp2->lastAccessTime;
	return p1 <= p2 ? item1 : item2;
}


/*********************************MRU priority interface************************************************/


void* MRU_initPriority(RankCache_t* cache, RankCache_Item_t* item) {
	MRU_Priority_t* p = malloc(sizeof(MRU_Priority_t));
	p->lastAccessTime = cache->clock;
	return p;
} 

void MRU_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item) {
	MRU_Priority_t* p = (MRU_Priority_t*)(item->priority);
	p->lastAccessTime = cache->clock;
}


void MRU_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item) {
}


//only diffference between LRU is this comparison function
RankCache_Item_t* MRU_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2) {
	
	assert(item1 != NULL);
	assert(item2 != NULL);

	MRU_Priority_t* pp1 = (MRU_Priority_t*)(item1->priority);
	MRU_Priority_t* pp2 = (MRU_Priority_t*)(item2->priority);
	uint64_t p1 = pp1->lastAccessTime;
	uint64_t p2 = pp2->lastAccessTime;
	return p1 > p2 ? item1 : item2;
}


/*********************************in cache LFU priority interface************************************************/

void* In_Cache_LFU_initPriority(RankCache_t* cache, RankCache_Item_t* item) {
	In_Cache_LFU_Priority_t* p = malloc(sizeof(In_Cache_LFU_Priority_t));
	p->freqCnt = 0;
	p->lastAccessTime = cache->clock;
	return p;
} 

void In_Cache_LFU_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item) {
	In_Cache_LFU_Priority_t* p = (In_Cache_LFU_Priority_t*)(item->priority);
	p->freqCnt +=1;
}

void In_Cache_LFU_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item) {
}



RankCache_Item_t* In_Cache_LFU_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2) {
	
	assert(item1 != NULL);
	assert(item2 != NULL);

	In_Cache_LFU_Priority_t* pp1 = (In_Cache_LFU_Priority_t*)(item1->priority);
	In_Cache_LFU_Priority_t* pp2 = (In_Cache_LFU_Priority_t*)(item2->priority);
	uint64_t p1 = pp1->freqCnt;
	uint64_t p2 = pp2->freqCnt;
	if (p1 == p2) return pp1->lastAccessTime <= pp2->lastAccessTime ? item1 : item2;
	return p1 < p2 ? item1 : item2;
}

/****************************perfect lfu priority interface************************************************/
void Perfect_LFU_globalDataInit(RankCache_t* cache) {
	Perfect_LFU_globalData* gd = malloc(sizeof(Perfect_LFU_globalData));

	gd->EvictedItem_HashTable = NULL;

	cache->globalData = (void*)gd;
}

void Perfect_LFU_globalDataFree(RankCache_t* cache) {
	Perfect_LFU_globalData* gd = (Perfect_LFU_globalData*)cache->globalData;
	Perfect_LFU_freqNode *currItem, *tmp;

	HASH_ITER(freq_hh, gd->EvictedItem_HashTable, currItem, tmp) {
		HASH_DELETE(freq_hh,gd->EvictedItem_HashTable, currItem);  /* delete; users advances to next */
		free(currItem);           /* optional- if you want to free  */
	}
}

//on init check whether it is in evicted table
// if it is, use the old freq cnt
void* Perfect_LFU_initPriority(RankCache_t* cache, RankCache_Item_t* item) {
	

	Perfect_LFU_Priority_t* p = malloc(sizeof(Perfect_LFU_Priority_t));
	
	Perfect_LFU_freqNode* temp;
	HASH_FIND(freq_hh, ((Perfect_LFU_globalData*)(cache->globalData))->EvictedItem_HashTable, &(item->key), sizeof(uint64_t), temp);
	if (temp == NULL) {
		p->freqCnt = 0;
	} else {
		p->freqCnt = temp->freqCnt;
	}
	p->lastAccessTime = cache->clock;

	return p;
} 

void Perfect_LFU_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item) {
	Perfect_LFU_Priority_t* p = (Perfect_LFU_Priority_t*)(item->priority);
	p->freqCnt +=1;
}

void Perfect_LFU_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item) {
//dup Item then store it to 
	Perfect_LFU_freqNode* temp;
	HASH_FIND(freq_hh, ((Perfect_LFU_globalData*)(cache->globalData))->EvictedItem_HashTable, &(item->key), sizeof(uint64_t), temp);
	if (temp == NULL) {
		temp = malloc(sizeof(Perfect_LFU_freqNode));
		temp->key = item->key;
		HASH_ADD(freq_hh, ((Perfect_LFU_globalData*)(cache->globalData))->EvictedItem_HashTable, key, sizeof(uint64_t), temp);
	}
	temp->freqCnt = ((Perfect_LFU_Priority_t*)(item->priority))->freqCnt;
}



RankCache_Item_t* Perfect_LFU_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2) {
	
	assert(item1 != NULL);
	assert(item2 != NULL);

	Perfect_LFU_Priority_t* pp1 = (Perfect_LFU_Priority_t*)(item1->priority);
	Perfect_LFU_Priority_t* pp2 = (Perfect_LFU_Priority_t*)(item2->priority);
	uint64_t p1 = pp1->freqCnt;
	uint64_t p2 = pp2->freqCnt;
	if (p1 == p2) return pp1->lastAccessTime <= pp2->lastAccessTime ? item1 : item2;
	return p1 <= p2 ? item1 : item2;
}