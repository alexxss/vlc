#!/bin/bash
echo "--------------------THIS SCRIPT SHOULD BE RUN INSIDE ./waf shell!!--------------------"

logfile="./log/automate.log"
varname="UE_number"
RBmode="GC"
TDMAmode="RND"
RAmode="HR"
prefix="${varname}-${RBmode}-${TDMAmode}-${RAmode}"
echo "Variable: $varname ..." | tee $logfile 
for filename in ./log/${prefix}_*.csv; do
	rm -- "$filename"
	echo "Removed ${filename}"
done

for var in {10..70..10} # ue from 10 to 70, step 10
do
	# (use tee so that it shows up in console AND log)
	echo "-------------------------$varname = $var-----------------------------" | tee -a $logfile
	printf "$var" | tee -a ./log/${prefix}_goodput.csv ./log/${prefix}_time.csv ./log/${prefix}_shannon.csv ./log/${prefix}_dropRate.csv ./log/${prefix}_aveConnNum.csv ./log/${prefix}_power.csv> /dev/null
	for ep in {1..20..2} # repeat simulation 20 times
	do
		# execute two simulation at once:
		echo "epoch $ep" ; ./build/release/scratch/thesis/thesis --Var_name=$varname --$varname=$var --RA_mode=$RAmode>> $logfile & \
		echo "epoch $(($ep+1))" ; ./build/release/scratch/thesis/thesis --Var_name=$varname --$varname=$var --RA_mode=$RAmode>> $logfile
		wait
	done
	printf "\n" | tee -a ./log/${varname}_goodput.csv ./log/${varname}_time.csv ./log/${varname}_shannon.csv ./log/${varname}_dropRate.csv ./log/${varname}_aveConnNum.csv ./log/${prefix}_power.csv> /dev/null
done

