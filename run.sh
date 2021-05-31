#!/bin/bash


InputPath=$HOME/Documents/Computer_Science/research/caching_system/traces/MSR-Cam_traces/filtered_msr/realSize/first_size
OutPath=.
KRR_VAR_=./mrc.sh
# EXACT_KLRU=$HOME/Documents/Computer_Science/research/LRU_mrc/MRC-Research/scripts/rand_evt_mrc.sh
Traces=('hm' 
		'mds' 
		'prn' 
		'proj'
		'prxy' 
		'rsrch' 
		'src1' 
		'src2' 
		'stg' 
		'ts' 
		'usr' 
		'wdev' 
		'web')

#init 32 step 32 final:
declare -A Hist_binSize

Hist_binSize=(
			['hm']='584000'
			['mds']='9552000'
			['prn']='11370000'
			['proj']='155600000'
			['prxy']='1858000'
			['rsrch']='173000'
			['src1']='35000000'
			['src2']='8000000'
			['stg']='9400000'
			['ts']='190000'
			['usr']='112600000'
			['wdev']='113000'
			['web']='8510000'
			)


# TRss=(1 2 4 8 16)
TRss=(1 2 4 8 16 32)
Hist_num=10000


#generate MRC for every single one
flag=0

for trace in ${Traces[@]}
do
	for ss in ${TRss[@]}
	do
		# $KRR_VAR_ "$InputPath"/"$trace"_msrSize.csv "$OutPath"/ $Hist_Init ${TracesSize[$trace]} $Hist_Step $ss 1.0 0 &
		# echo ${Hist_binSize[$trace]}0000
		$KRR_VAR_ "$InputPath"/"$trace"_msrSize.csv ${Hist_binSize[$trace]}0000 50 5 lru $ss 0.0 &
		flag=$flag+1
		if [ $((flag%1)) -eq 0 ]
		then
			wait
		fi
		# $EXACT_KLRU "$InputPath"/"$trace".csv $ss ${TracesSize[$trace]} 100 4 &
		
		# wait
	done
done
