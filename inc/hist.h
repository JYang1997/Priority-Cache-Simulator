#ifndef JY_HIST_H
#define JY_HIST_H
#define COLDMISS -1234

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


#endif /*JY_HIST_H*/