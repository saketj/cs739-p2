#!/bin/bash

WORKING_DIR=~/Development/projects/cs739-p2/nfs

MTBF=1
MTTR=0.1

MAX=10000

echo "Running server with availability of `bc <<< 'scale=2; ((1-0.1)/1)*100'`%"

i=0
while [ $i -lt $MAX ]
do
  source $WORKING_DIR/setup-env-vars.sh
  $WORKING_DIR/nfs_server.out &   # Launch the server
  sleep $MTBF
  kill -9 `pgrep nfs_server`
  sleep $MTTR
  i=$[$i+1]
done
