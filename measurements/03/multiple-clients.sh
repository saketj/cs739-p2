#!/bin/bash

FAIL=0

echo "Starting..."
x=`date +%s%N`
sleep 5 &
sleep 2 &
sleep 3 &
sleep 1 &

for job in `jobs -p`
do
    wait $job || let "FAIL+=1"
done
y=`date +%s%N`

echo "$FAIL jobs failed."

if [ "$FAIL" == "0" ];
then
echo "All done successfully!"
else
echo "FAIL! ($FAIL)"
fi

echo `expr $y - $x`
