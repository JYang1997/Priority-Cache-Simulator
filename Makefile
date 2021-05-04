IDIR1 = ./inc
IDIR2 = ../inc
CFLAGS=-I$(IDIR1) -I$(IDIR2)
CC = gcc
MAIN = ./src/main.c
SRC = ./src/hist.c ./src/priority.c ./src/RankCache.c 
DOTO = $(IDIR2)/murmur3.o $(IDIR2)/entropy.o $(IDIR2)/libpcg_random.a


all: CACHE

CACHE: $(MAIN)
	$(CC) $(CFLAGS) $(MAIN) $(SRC) $(DOTO) -O2 -g -lm -o rk_cache

clean:
	rm -f rk_cache


# gcc -I../../dev/inc -I../../dev/priority_cache/inc/ -I../../traces/twitter-trace/  
# main.c ../../traces/twitter-trace/twitter_2020.c 
# ../../dev/priority_cache/src/hist.c ../../dev/priority_cache/src/priority.c 
# ../../dev/priority_cache/src/RankCache.c ../../dev/inc/murmur3.o 
# ../../dev/inc/entropy.o ../../dev/inc/libpcg_random.a -g3 -lm -o tw_cache