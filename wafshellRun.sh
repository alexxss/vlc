#!/bin/bash
echo "--------------------THIS SCRIPT SHOULD BE RUN INSIDE ./waf shell!!--------------------"
echo "Remeber to set RBmode, TDMAmode, RAmode..."
echo ""

logfile="./log/automate.log"
varname="UE_number"
RBmode="WGC"
TDMAmode="GA"
RAmode="HR"
prefix="${varname}-${RBmode}-${TDMAmode}-${RAmode}"
echo "Variable: $varname ..." | tee $logfile 
echo "Output file prefix: ${prefix}"
for filename in ./log/${prefix}_*.csv; do
	rm -- "$filename"
	echo "Removed ${filename}"
done

for var in {10..70..10} # paramX from 1 to 3 step 1
do
	# (use tee so that it shows up in console AND log)
	echo "-------------------------$varname = $var-----------------------------" | tee -a $logfile
	printf "$var" | tee -a ./log/${prefix}_goodput.csv ./log/${prefix}_time.csv ./log/${prefix}_shannon.csv ./log/${prefix}_dropRate.csv ./log/${prefix}_aveConnNum.csv ./log/${prefix}_power.csv ./log/${prefix}_pruneCnt.csv > /dev/null
	for ep in {1..40..2} # repeat simulation 20 times
	do
		# execute two simulation at once:
		echo "epoch $ep" ; ./build/release/scratch/thesis/thesis --Var_name=$varname --$varname=$var --RB_mode=$RBmode --TDMA_mode=$TDMAmode --RA_mode=$RAmode>> $logfile & \
		echo "epoch $(($ep+1))" ; ./build/release/scratch/thesis/thesis --Var_name=$varname --$varname=$var --RB_mode=$RBmode --TDMA_mode=$TDMAmode --RA_mode=$RAmode>> $logfile 
		wait
	done
	printf "\n" | tee -a ./log/${prefix}_goodput.csv ./log/${prefix}_time.csv ./log/${prefix}_shannon.csv ./log/${prefix}_dropRate.csv ./log/${prefix}_aveConnNum.csv ./log/${prefix}_power.csv ./log/${prefix}_pruneCnt.csv > /dev/null
done
echo "!!!!!!!!!LAMBDA IS 3!! DON'T OVERWRITE!!!!!!!"
echo "WGC HR prune count"
