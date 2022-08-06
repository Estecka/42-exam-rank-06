#!/bin/bash

{ cat -e </dev/urandom | tr -d "\n" | head -c $2; echo; } >./biginbuff.log;

timeout 30s ./miniserv $1 >serv.log & SERVER=$!
sleep 1;
nc -w 2 localhost $1 >bigoutraw.log & LISTENER=$!
sleep 1;
nc -w 0 localhost $1 <biginbuff.log & EMITTER=$!

wait $EMITTER $LISTENER;
kill $SERVER

grep -v "^server: " <bigoutraw.log | sed -r "s/^client [0-9]+ : //gm" >bigoutbuff.log

diff biginbuff.log bigoutbuff.log && echo "No diff" || { echo "Diff"; exit 1; }