device_total=10
rule_total=1
rule_max_device=1
device_max_state=5

device=""
COUNTER=0  
device_t="device:"
space=" "
while [  $COUNTER -lt $rule_total ]; do  
	all=`expr $RANDOM % $rule_max_device`;
	while [ $all -ge 0 ]; do
		sum=`expr $RANDOM % $device_total`;
		value=`expr $RANDOM % $device_max_state`;
	   	device=$device$device_t$sum$space$value$space
	   	let all=all-1
	done	
	echo " "
	echo $device
	./a.out $device
	device=""
    let COUNTER=COUNTER+1   
	echo $COUNTER
done  
