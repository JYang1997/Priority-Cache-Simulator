#include "../inc/hist.h"
#include <stdlib.h>



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

