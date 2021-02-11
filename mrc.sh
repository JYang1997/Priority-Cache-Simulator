#Simple script for generating fixed interval mrc for different policies

#arg1 file name
#arg2 distinct object
#arg3 total point
#arg4 process num
#arg5 total reqs num, 0 let cache auto search

#!/bin/bash

filename=$1
obj=$2
pntNum=$3
proNum=$4
policy=$5
sam=$6




if [[ $# -ne 6 ]]; then
	echo "Illegal number of parameters"
	echo "arg1 file, arg2 cmaxSize, arg3 #intervals, arg4 proNum, arg5 policy, arg6 sample size"
	echo "policies: lru, lhd, lfu, hc. sample size = 1 is random eviciton"
	exit
fi

# if [[ $obj -lt 10000 ]]; then
# 	echo "illegal arguments more obj needed"
# 	exit
# fi

# obj=$(((obj * 4) / 3))
step=$((obj / pntNum))
interval=$((step * proNum))
cSize=$step
echo $interval
echo $obj
echo $cSize



for (( j = $interval; j <= $obj; j+=$interval )); do
	
	for (( i=0 ; $cSize <= j; cSize+=$step )); do

		output="${filename}_${sam}_${policy}.mrc"
		./rk_cache $filename $cSize ${sam} ${policy} 1 0 2>> $output &

	done		
		
	wait
	echo $cSize 
done


