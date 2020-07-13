#!/bin/bash
echo "THIS SCRIPT SHOULD BE RUN INSIDE ./waf shell!!"

logfile="./log/automate.log"
varname="UE_number"
echo "Doing experiments with $varname as variable..." | tee $logfile 
for filename in ./log/${varname}_*.csv; do
	rm -- "$filename"
	echo "Removed ${filename}"
done

for var in {10..70..10} # ue from 10 to 70, step 10
do
	echo "-------------------------$varname = $var-----------------------------" | tee -a $logfile
	printf "$var " | tee -a ./log/${varname}_goodput.csv ./log/${varname}_time.csv ./log/${varname}_shannon.csv > /dev/null
	for ep in {1..20} # repeat simulation 20 times
	do
		echo "epoch $ep"
		# execute simulation:
		./build/scratch/thesis/thesis --Var_name=$varname --$varname=$var >> $logfile
	done
	printf "\n" | tee -a ./log/${varname}_goodput.csv ./log/${varname}_time.csv ./log/${varname}_shannon.csv > /dev/null

done

