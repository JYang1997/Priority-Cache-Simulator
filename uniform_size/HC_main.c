
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include "RankCache.h"
#include <assert.h>

int TIME_FLAG = 0;
double tt_time =0;

typedef struct _HC_Priority_t 
{
	uint32_t refCount;
	uint32_t lastAccessTime;
	
} HC_Priority_t;


typedef struct _RedisLFU_Priority_t
{
	uint32_t refCount;

} RedisLFU_Priority_t;


void* HC_initPriority(RankCache_t* cache, RankCache_Item_t* item) {

	HC_Priority_t* p = malloc(sizeof(HC_Priority_t));
	p->refCount = 0;
	p->lastAccessTime = cache->clock;

	return p;
} 

void HC_updatePriority(RankCache_t* cache, RankCache_Item_t* item) {

	HC_Priority_t* p = (HC_Priority_t*)(item->priority);
	p->refCount++;
	// p->lastAccessTime = cache->clock;

}

RankCache_Item_t* HC_minPriorityItem(RankCache_t* cache, RankCache_Item_t* item1, RankCache_Item_t* item2) {

	assert(item1 != NULL);
	assert(item2 != NULL);
	double p1, p2;
	HC_Priority_t* pp1 = (HC_Priority_t*)(item1->priority);
	HC_Priority_t* pp2 = (HC_Priority_t*)(item2->priority);
	p1 = (pp1->refCount) / (double)(cache->clock - pp1->lastAccessTime);
	p2 = (pp2->refCount) / (double)(cache->clock - pp2->lastAccessTime);

	return p1 <= p2 ? item1 : item2;
}



// RedisLFU_Priority_t* RedisLFU_initPriority(RankCache_t* cache, RankCache_Item_t* item) {

// } 

// void RedisLFU_updatePriority(RankCache_t* cache, RankCache_Item_t* item) {

// }

// RankCache_Item_t* RedisLFU_maxPriorityItem(RankCache_Item_t* item1, RankCache_Item_t* item2) {


// }





int trace_processing(RankCache_t* cache, FILE* fd) {

	char *keyStr;
	uint64_t key;
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
		key = strtoull(line, NULL, 10);
		// keyStr = strtok(line, "\n");
		
		struct timeval  tv1, tv2;
		gettimeofday(&tv1, NULL);

		if (access(cache, key, 1) == CACHE_MISS)	totMiss++;
		
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
	RankCache_t *cache = cacheInit(strtoul(argv[2], NULL, 10),
								   strtoul(argv[3], NULL, 10),
								   HC_initPriority,
								   HC_updatePriority,
								   HC_minPriorityItem);
	gettimeofday(&tv2, NULL);
	tt_time += (double) (tv2.tv_usec - tv1.tv_usec) / 1000000 + (double) (tv2.tv_sec - tv1.tv_sec);


	int totMiss = trace_processing(cache, rfd);
	fprintf(stderr,"%d, %f\n",cache->capacity, totMiss/(double)cache->totRef);

	if(TIME_FLAG == 1)
		printf ("Total time = %f seconds\n", tt_time);

	cacheFree(cache);
	fclose(rfd);
	return 0;
}

