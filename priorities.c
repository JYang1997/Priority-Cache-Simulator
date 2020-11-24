#include "RankCache.h"
#include "hist.h"
#include <assert.h>



typedef struct _HC_Priority_t 
{
	uint32_t refCount;
	uint32_t enterTime;
	
} HC_Priority_t;


typedef struct _RedisLFU_Priority_t
{
	uint32_t refCount;

} RedisLFU_Priority_t;


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


/**
 *LHD implementation
 * LHD require two tables for compute it's rank
 * 1. hit distribution 2.lifetime distribution
 * a. in there implementation, they precomput a table for lhd vs age
 * then rank calculation is simply a lookup
 * for now, we will explicitly compute the the LHD for every lookup
 * for actual usage periodically updated lookup table needed
 *
 * hit distribution, is reuse time histogram
 * life time distribution, record both hit and evict of age a.
 *
 * for each item, we will need a field to store the last access time
 * then age is simply number of access since last accessed, 
 * note this is not reuse time, because age is collected at eviction
 *  
 */

Hist_t *hitHist = NULL;
Hist_t *lifeTimeHist = NULL;
Hist_t *lhdHist = NULL;
uint32_t lastUpdateTime = 0;
int lhd_period = 100000;

typedef struct _LHD_Priority_t
{
	uint32_t lastAccessTime;

} LHD_Priority_t;



void* LHD_initPriority(RankCache_t* cache, RankCache_Item_t* item) {
	LHD_Priority_t* p = malloc(sizeof(LHD_Priority_t));
	p->lastAccessTime = cache->clock;
	addToHist(hitHist, COLDMISS);
	addToHist(lifeTimeHist, COLDMISS);
	return p;
} 

void LHD_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item) {
	LHD_Priority_t* p = (LHD_Priority_t*)(item->priority);

	uint32_t age = (cache->clock)-(p->lastAccessTime);
	addToHist(hitHist, age);
	addToHist(lifeTimeHist, age);
	
	p->lastAccessTime = cache->clock;
}


void LHD_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item) {
	LHD_Priority_t* p = (LHD_Priority_t*)(item->priority);
	uint32_t age = (cache->clock)-(p->lastAccessTime);
	addToHist(lifeTimeHist, age);

}

void updateLHDHist() {
	assert(lhdHist->size > 0);
	
	//include cold miss? or not
	double acc_Hit = hitHist->Hist[lhdHist->size-1];
	double acc_lifetime = lifeTimeHist->Hist[lhdHist->size-1];
	double exp_lifetime = acc_lifetime;

	lhdHist->Hist[lhdHist->size-1] = (acc_Hit/(hitHist->tot))/(exp_lifetime/(lifeTimeHist->tot));

	for (int i = lhdHist->size-2; i >= 0 ; --i)
	{

		lhdHist->Hist[i] = (acc_Hit/(hitHist->tot))/(exp_lifetime/(lifeTimeHist->tot));
		acc_Hit += hitHist->Hist[i];
		acc_lifetime += lifeTimeHist->Hist[i];
		exp_lifetime  += acc_lifetime;
	}
}

RankCache_Item_t* LHD_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2) {
	
	assert(item1 != NULL);
	assert(item2 != NULL);
	
	//check when is last time update the table
	//if 10000 reference passed, update it
	if(cache->clock - lhd_period > lastUpdateTime){
		lastUpdateTime = cache->clock;
		updateLHDHist();
	}

	double p1, p2;
	LHD_Priority_t* pp1 = (LHD_Priority_t*)(item1->priority);
	LHD_Priority_t* pp2 = (LHD_Priority_t*)(item2->priority);

	//evict the one with low priority
	uint32_t age1 = (cache->clock)-(pp1->lastAccessTime);
	uint32_t age2 = (cache->clock)-(pp2->lastAccessTime);

	int lhd_index1 = (((int)age1 - lhdHist->first) / (lhdHist->interval)) + 1;
	int lhd_index2 = (((int)age2 - lhdHist->first) / (lhdHist->interval)) + 1;

	if(lhd_index1 > lhdHist->size-1) {
		lhd_index1 = lhdHist->size-1;
	}

	if(lhd_index2 > lhdHist->size-1) {
		lhd_index2 = lhdHist->size-1;
	}


	p1 = lhdHist->Hist[lhd_index1]/(item1->size);
	p2 = lhdHist->Hist[lhd_index2]/(item2->size);

	return p1 <= p2 ? item1 : item2;
}



typedef struct _LRU_Priority_t
{
	uint32_t lastAccessTime;

} LRU_Priority_t;



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
	uint32_t p1 = pp1->lastAccessTime;
	uint32_t p2 = pp2->lastAccessTime;
	return p1 <= p2 ? item1 : item2;
}



typedef struct _LFU_Priority_t
{
	uint32_t freqCnt;

} LFU_Priority_t;



void* LFU_initPriority(RankCache_t* cache, RankCache_Item_t* item) {
	LFU_Priority_t* p = malloc(sizeof(LFU_Priority_t));
	p->freqCnt = 0;
	return p;
} 

void LFU_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item) {
	LFU_Priority_t* p = (LFU_Priority_t*)(item->priority);
	p->freqCnt += 1;
}

void LFU_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item) {
}



RankCache_Item_t* LFU_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2) {
	
	assert(item1 != NULL);
	assert(item2 != NULL);

	LFU_Priority_t* pp1 = (LFU_Priority_t*)(item1->priority);
	LFU_Priority_t* pp2 = (LFU_Priority_t*)(item2->priority);
	uint32_t p1 = pp1->freqCnt;
	uint32_t p2 = pp2->freqCnt;
	return p1 <= p2 ? item1 : item2;
}