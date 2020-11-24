
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include "RankCache.h"
#include <assert.h>

int TIME_FLAG = 0;
double tt_time =0;


typedef struct Hist_t {
	double* Hist; //contain first to last + coldmiss box
	int first;
	int size; 
	int interval;
	int tot; //sum of all bin
} Hist_t;

Hist_t* histInit(int begin, int end, int interval);
void histFree(Hist_t* hist);
void addToHist(Hist_t* hist, long long age);

Hist_t *hitHist = NULL;
Hist_t *lifeTimeHist = NULL;
Hist_t *lhdHist = NULL;
uint32_t lastUpdateTime = 0;
int lhd_period = 100000;
int upcnt = 0;

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



Hist_t* histInit(int begin, int end, int interval)
{
	//consider edge case later
	Hist_t* hist = malloc(sizeof(Hist_t));
	int slots = ((end - begin)/interval);
	hist -> first = begin;
	hist -> size = slots+2;
	hist -> interval = interval;
	hist -> tot = 0;
	hist -> Hist = (double*)malloc(sizeof(double)*(slots+2));

	int i;
	for(i = 0; i < slots+2; i++)
	{
		hist -> Hist[i] = 0;
	}

	return hist;
}


void histFree(Hist_t* hist)
{
	free((void*)hist->Hist);
}


void addToHist(Hist_t* hist, long long age)
{
	hist->tot += 1;
	if(age == COLDMISS || age > (((hist->size)-2)*(hist->interval)+(hist->first))){

		hist -> Hist[hist->size-1]++; //cold miss bin++
	}else{
		if(age - hist->first >= 0)
		{
			long long index = ((age - hist->first) / (hist->interval)) + 1;
			hist->Hist[index]++;
		}else{
			hist->Hist[0]++;
		}
	}
}



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
	if (cache->clock - lhd_period > lastUpdateTime){
		lastUpdateTime = cache->clock;
		updateLHDHist();
		upcnt++;
	//	printf("%lld\n", cache->totRef);
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

	//both mul hist step = not
	//p1 = lhdHist->Hist[lhd_index1]/(histstep*(item1->size));
	//p2 = lhdHist->Hist[lhd_index2]/(histstep*(item2->size));

	p1 = lhdHist->Hist[lhd_index1]/(item1->size);
	p2 = lhdHist->Hist[lhd_index2]/(item2->size);

	//printf("age1: %d p1 %f age2: %d p2 %f tot: %lld\n",age1, p1, age2, p2,  cache->totRef);
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
// RedisLFU_Priority_t* RedisLFU_initPriority(RankCache_t* cache, RankCache_Item_t* item) {

// } 

// void RedisLFU_updatePriority(RankCache_t* cache, RankCache_Item_t* item) {

// }

// RankCache_Item_t* RedisLFU_maxPriorityItem(RankCache_Item_t* item1, RankCache_Item_t* item2) {


// }


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
	p->freqCnt +=1;
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


int trace_processing(RankCache_t* cache, FILE* fd) {

	char *keyStr;
	uint64_t key;
	char *sizeStr;
	uint32_t size;
	char* ret;
	char   line[256];

	int totMiss = 0;
	///////////////////progress bar////////////////////////////
	char bar[28];
	int i;
	uint32_t d = 0;
	//first line contain total reference number
	bar[0] = '[';
	bar[26] = ']';
	bar[27]='\0';
	for (i=1; i<=25; i++) bar[i] = ' ';
	ret=fgets(line, 256, fd);
	keyStr = strtok(line, " ");
	keyStr = strtok(NULL, " ");
	uint64_t total = strtoull(keyStr, NULL, 10);
	uint64_t star = total/100;
	///////////////////////////////////////////////////////////
	i=1;
	while ((ret=fgets(line, 256, fd)) != NULL)
	{
		keyStr = strtok(line, ",");
		key = strtoull(keyStr, NULL, 10);
		sizeStr = strtok(NULL, ",");
		if (sizeStr != NULL)	
			size = strtoul(sizeStr, NULL, 10);
		else
			size = 1;
		struct timeval  tv1, tv2;
		gettimeofday(&tv1, NULL);

		if (access(cache, key, size) == CACHE_MISS)	totMiss++;
		
		gettimeofday(&tv2, NULL);
		tt_time += (double) (tv2.tv_usec - tv1.tv_usec) / 1000000 + (double) (tv2.tv_sec - tv1.tv_sec);
		d++;
		
		if(d % star == 0)
		{	
			if( d%(star*4) == 0)
				bar[i++] = '#';
			printf("\rProgress: %s%d%% %d", bar, (int)(d/(double)total*100)+1, d);
			fflush(stdout);
		}
		
	}
	printf("\n");
	return totMiss;
}





int main(int argc, char const *argv[])
{
	// char buf[1024];


	char* input_format = "arg1: input filename\n \
arg2: Cache Size\n \
arg3: Sample Size\n \
arg4: timer flag (1 yes 0 no)\n";

	if(argc < 5) { 
		printf("%s", input_format);
		return 0;
	}
	//open file for read key value
	FILE*       rfd = NULL;
	if(atoi(argv[4]) == 1) TIME_FLAG = 1;
	else if(atoi(argv[4]) == 0) TIME_FLAG = 0;
	else printf("%s", input_format);

	if((rfd = fopen(argv[1],"r")) == NULL)
	{ perror("open error for read"); return -1; }
	

	struct timeval  tv1, tv2;
	gettimeofday(&tv1, NULL);
	//cache init
	//hist init temporary
	hitHist = histInit(128,1024*1024*10, 128);
	lifeTimeHist = histInit(128,1024*1024*10, 128);
	lhdHist = histInit(128,1024*1024*10, 128);
	RankCache_t *cache = cacheInit(strtoul(argv[2], NULL, 10),
								   strtoul(argv[3], NULL, 10),
								   LHD_initPriority,
								   LHD_updatePriorityOnHit,
								   LHD_updatePriorityOnEvict,
								   LHD_minPriorityItem);
	gettimeofday(&tv2, NULL);
	tt_time += (double) (tv2.tv_usec - tv1.tv_usec) / 1000000 + (double) (tv2.tv_sec - tv1.tv_sec);


	int totMiss = trace_processing(cache, rfd);
	// fprintf(stderr, "update %d\n", upcnt);
	fprintf(stderr,"%f, %f\n",cache->capacity, totMiss/(double)cache->totRef);

	if(TIME_FLAG == 1)
		printf ("Total time = %f seconds\n", tt_time);

	cacheFree(cache);
	fclose(rfd);
	return 0;
}

