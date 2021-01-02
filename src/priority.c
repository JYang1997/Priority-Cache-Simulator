#include "RankCache.h"
#include "hist.h"
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

Hist_t *hitHist = NULL;
Hist_t *lifeTimeHist = NULL;
Hist_t *lhdHist = NULL;
uint64_t lastUpdateTime = 0;
int lhd_period = 100000;

void* LHD_initPriority(RankCache_t* cache, RankCache_Item_t* item) {
	LHD_Priority_t* p = malloc(sizeof(LHD_Priority_t));
	p->lastAccessTime = cache->clock;
	addToHist(hitHist, COLDMISS);
	addToHist(lifeTimeHist, COLDMISS);
	return p;
} 

void LHD_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item) {
	LHD_Priority_t* p = (LHD_Priority_t*)(item->priority);

	uint64_t age = (cache->clock)-(p->lastAccessTime);
	addToHist(hitHist, age);
	addToHist(lifeTimeHist, age);
	
	p->lastAccessTime = cache->clock;
}


void LHD_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item) {
	LHD_Priority_t* p = (LHD_Priority_t*)(item->priority);
	uint64_t age = (cache->clock)-(p->lastAccessTime);
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
	if (cache->clock - lhd_period > lastUpdateTime){
		lastUpdateTime = cache->clock;
		updateLHDHist();
	//	printf("%lld\n", cache->totRef);
	}

	double p1, p2;
	LHD_Priority_t* pp1 = (LHD_Priority_t*)(item1->priority);
	LHD_Priority_t* pp2 = (LHD_Priority_t*)(item2->priority);

	//evict the one with low priority
	uint64_t age1 = (cache->clock)-(pp1->lastAccessTime);
	uint64_t age2 = (cache->clock)-(pp2->lastAccessTime);

	int lhd_index1 = (((int)age1 - lhdHist->first) / (lhdHist->interval)) + 1;
	int lhd_index2 = (((int)age2 - lhdHist->first) / (lhdHist->interval)) + 1;

	if(lhd_index1 > lhdHist->size-1) {
		lhd_index1 = lhdHist->size-1;
	}

	if(lhd_index2 > lhdHist->size-1) {
		lhd_index2 = lhdHist->size-1;
	}

	//both mul hist step = not
	//p1 = lhdHist->Hist[lhd_index1]/(histstep*(item1->size));
	//p2 = lhdHist->Hist[lhd_index2]/(histstep*(item2->size));

	p1 = lhdHist->Hist[lhd_index1]/(item1->size);
	p2 = lhdHist->Hist[lhd_index2]/(item2->size);

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




/*********************************LFU priority interface************************************************/

void* LFU_initPriority(RankCache_t* cache, RankCache_Item_t* item) {
	LFU_Priority_t* p = malloc(sizeof(LFU_Priority_t));
	p->freqCnt = 0;
	return p;
} 

void LFU_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item) {
	LFU_Priority_t* p = (LFU_Priority_t*)(item->priority);
	p->freqCnt +=1;
}

void LFU_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item) {
}



RankCache_Item_t* LFU_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2) {
	
	assert(item1 != NULL);
	assert(item2 != NULL);

	LFU_Priority_t* pp1 = (LFU_Priority_t*)(item1->priority);
	LFU_Priority_t* pp2 = (LFU_Priority_t*)(item2->priority);
	uint64_t p1 = pp1->freqCnt;
	uint64_t p2 = pp2->freqCnt;
	return p1 <= p2 ? item1 : item2;
}
