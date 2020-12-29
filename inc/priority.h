

/*********************************LRU priority interface************************************************/

typedef struct _LRU_Priority_t
{
	uint32_t lastAccessTime;

} LRU_Priority_t;

void* LRU_initPriority(RankCache_t* cache, RankCache_Item_t* item);
void LRU_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item);
void LRU_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item);
RankCache_Item_t* LRU_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2);


/*********************************LFU priority interface************************************************/


typedef struct _LFU_Priority_t
{
	uint32_t freqCnt;

} LFU_Priority_t;



void* LFU_initPriority(RankCache_t* cache, RankCache_Item_t* item);
void LFU_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item);
void LFU_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item);

RankCache_Item_t* LFU_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2);


/*********************************HC priority interface************************************************/

typedef struct _HC_Priority_t 
{
	uint32_t refCount;
	uint32_t enterTime;
	
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

Hist_t *hitHist = NULL;
Hist_t *lifeTimeHist = NULL;
Hist_t *lhdHist = NULL;
uint32_t lastUpdateTime = 0;
int lhd_period = 100000;
int upcnt = 0;

typedef struct _LHD_Priority_t
{
	uint32_t lastAccessTime;

} LHD_Priority_t;

void* LHD_initPriority(RankCache_t* cache, RankCache_Item_t* item);
void LHD_updatePriorityOnHit(RankCache_t* cache, RankCache_Item_t* item);
void LHD_updatePriorityOnEvict(RankCache_t* cache, RankCache_Item_t* item);
RankCache_Item_t* LHD_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2);

