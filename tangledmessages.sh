#!/bin/bash

cat -e </dev/urandom | tr -d "\n" | head -c $2 >./biginbuff.log;

echo -n "client 1 : " > expected.log
cat biginbuff.log     >>expected.log
echo -n "client 2 : " >>expected.log
cat biginbuff.log     >>expected.log
echo                  >>expected.log

timeout 30s ./miniserv $1 >serv.log & SERVER=$!
sleep 0.5
nc -w 2 localhost $1 >bigoutraw.log & LISTENER=$!
sleep 0.5;
{ sleep 1; cat biginbuff.log; }       | nc -w 2 localhost $1 >/dev/null & EMITTER1=$!
sleep 0.5;
{ sleep 2; cat biginbuff.log; echo; } | nc -w 2 localhost $1 >/dev/null & EMITTER2=$!

wait $EMITTER1 $EMITTER2 $LISTENER;
kill $SERVER

grep -v "^server: " <bigoutraw.log >bigoutbuff.log

diff expected.log bigoutbuff.log && echo "No diff" || { echo "Diff"; exit 1; }