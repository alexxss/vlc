#!/bin/bash

### FUNCTION FOR FILES ###
writeToCsvAndConsole(){
#	files=
	outputText=$1 #argument 1: text to output to ALL the files
	prefix=$2 #argument 2: file prefix
	#echo "$outputText"
	for file in 'goodput' 'time' 'shannon' 'dropRate' 'power' 'dropTrigCnt' 'oldBreakTrigCnt' 'blockedBandwidth' 'totalReqBandwidth' 'pruneCnt'
	do
		#echo "./log/${prefix}_${file}.csv"
		printf "$outputText" >> ./log/${prefix}_${file}.csv
	done
}


echo "--------------------THIS SCRIPT SHOULD BE RUN INSIDE ./waf shell!!--------------------"
echo "Remeber to set RBmode, TDMAmode, RAmode..."
echo ""

logfile="./log/automate.log"
varname="UE_number"
if (( $# < 3 )); then
# default
	echo "No cmd args / Not enough args. Default mode GC-GA-BP."
	RBmode="GC"
	TDMAmode="GA"
	RAmode="BP"
else
# read modes from cmd arg
	RBmode=$1; shift
	TDMAmode=$1; shift
	RAmode=$1; shift
fi

prefix="${varname}-${RBmode}-${TDMAmode}-${RAmode}"
echo "Variable: $varname ..." | tee $logfile 
echo "Output file prefix: ${prefix}"
for filename in ./log/${prefix}_*.csv; do
	rm --verbose -- "$filename"
done

for var in {10..70..10} # UE_number from 10 to 70 step 10
do
	# (use tee so that it shows up in console AND log)
	echo "-------------------------$varname = $var-----------------------------" | tee -a $logfile
	#printf "$var" | tee -a ./log/${prefix}_goodput.csv ./log/${prefix}_time.csv ./log/${prefix}_shannon.csv ./log/${prefix}_dropRate.csv ./log/${prefix}_aveConnNum.csv ./log/${prefix}_power.csv ./log/${prefix}_dropTrigCnt.csv ./log/${prefix}_oldBreakTrigCnt.csv > /dev/null
	writeToCsvAndConsole "$var" ${prefix}
	for ep in {1..40..2} # repeat simulation 40 times
	do
		# execute two simulation at once:
		echo "epoch $ep" ; ./build/release/scratch/thesis/thesis --Var_name=$varname --$varname=$var --RB_mode=$RBmode --TDMA_mode=$TDMAmode --RA_mode=$RAmode>> $logfile & \
		echo "epoch $(($ep+1))" ; ./build/release/scratch/thesis/thesis --Var_name=$varname --$varname=$var --RB_mode=$RBmode --TDMA_mode=$TDMAmode --RA_mode=$RAmode>> $logfile 
		wait
	done
	#printf "\n" | tee -a ./log/${prefix}_goodput.csv ./log/${prefix}_time.csv ./log/${prefix}_shannon.csv ./log/${prefix}_dropRate.csv ./log/${prefix}_aveConnNum.csv ./log/${prefix}_power.csv ./log/${prefix}_dropTrigCnt.csv ./log/${prefix}_oldBreakTrigCnt.csv > /dev/null
	writeToCsvAndConsole "\n" ${prefix}
done

if [ ! -d log/$varname ]; then
	mkdir log/$varname
fi
mv log/*.csv log/$varname/
echo "moved to log/$varname/"
