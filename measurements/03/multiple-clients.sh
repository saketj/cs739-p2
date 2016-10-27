#!/bin/bash

FAIL=0

echo "Starting..."

sleep 5 &
sleep 2 &
sleep 3 &
sleep 1 &

echo "Launched."

for job in `jobs -p`
do
    wait $job || let "FAIL+=1"
done

echo "$FAIL jobs failed."

if [ "$FAIL" == "0" ];
then
echo "All done successfully!"
else
echo "FAIL! ($FAIL)"
fi
