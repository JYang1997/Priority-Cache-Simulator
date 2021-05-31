
#ifndef JY_PRIORITY_H
#define JY_PRIORITY_H
/*********************************LRU priority interface************************************************/

typedef struct _LRU_Priority_t
{
	uint64_t lastAccessTime;

} LRU_Priority_t;

void* LRU_initPriority(RankCache_t* cache, RankCache_Item_t* item);
void LRU_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item);
void LRU_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item);
RankCache_Item_t* LRU_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2);



/*********************************MRU priority interface************************************************/

typedef struct _MRU_Priority_t
{
	uint64_t lastAccessTime;

} MRU_Priority_t;

void* MRU_initPriority(RankCache_t* cache, RankCache_Item_t* item);
void MRU_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item);
void MRU_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item);
RankCache_Item_t* MRU_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2);


/*********************************incache LFU priority interface************************************************/


typedef struct _In_Cache_LFU_Priority_t
{
	uint64_t freqCnt;
	uint64_t lastAccessTime;

} In_Cache_LFU_Priority_t;



void* In_Cache_LFU_initPriority(RankCache_t* cache, RankCache_Item_t* item);
void In_Cache_LFU_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item);
void In_Cache_LFU_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item);

RankCache_Item_t* In_Cache_LFU_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2);




/*********************************perfect LFU priority interface************************************************/
//must store deleted item into hash table
typedef struct _Perfect_LFU_freqNode {
	uint64_t key;
	uint64_t freqCnt;
	UT_hash_handle freq_hh; 
} Perfect_LFU_freqNode;

typedef struct _Perfect_LFU_globalData {
	Perfect_LFU_freqNode *EvictedItem_HashTable;

} Perfect_LFU_globalData;

void Perfect_LFU_globalDataInit(RankCache_t* cache);
void Perfect_LFU_globalDataFree(RankCache_t* cache);

typedef struct _Perfect_LFU_Priority_t
{
	uint64_t freqCnt;
	uint64_t lastAccessTime;

} Perfect_LFU_Priority_t;



void* Perfect_LFU_initPriority(RankCache_t* cache, RankCache_Item_t* item);
void Perfect_LFU_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item);
void Perfect_LFU_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item);

RankCache_Item_t* Perfect_LFU_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2);

/*********************************pseudo-perfect LFU priority interface************************************************/
//use logarithmic counter of max 255

typedef struct _Pseudo_Perfect_LFU_Priority_t
{
	uint64_t freqCnt;
	uint64_t lastAccessTime;

} Pseudo_Perfect_LFU_Priority_t;



void* Pseudo_Perfect_LFU_initPriority(RankCache_t* cache, RankCache_Item_t* item);
void Pseudo_Perfect_LFU_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item);
void Pseudo_Perfect_LFU_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item);

RankCache_Item_t* Pseudo_Perfect_LFU_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2);



/*********************************HC priority interface************************************************/

typedef struct _HC_Priority_t 
{
	uint64_t refCount;
	uint64_t enterTime;
	
} HC_Priority_t;

void* HC_initPriority(RankCache_t* cache, RankCache_Item_t* item);
void HC_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item) ;
void HC_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item);
RankCache_Item_t* HC_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2);


/*********************************LHD priority interface************************************************/
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


// extern Hist_t *hitHist;
// extern Hist_t *lifeTimeHist;
// extern Hist_t *lhdHist;
// extern uint64_t lastUpdateTime;
// extern int lhd_period;

typedef struct _LHD_globalData {
	Hist_t *hitHist;
	Hist_t *lifeTimeHist;
	Hist_t *lhdHist;
	uint64_t lastUpdateTime;
	int lhd_period;
} LHD_globalData;

void LHD_globalDataInit(RankCache_t* cache);

typedef struct _LHD_Priority_t
{
	uint64_t lastAccessTime;

} LHD_Priority_t;

void* LHD_initPriority(RankCache_t* cache, RankCache_Item_t* item);
void LHD_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item);
void LHD_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item);
RankCache_Item_t* LHD_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2);

#endif /*JY_PRIORITY_H*/