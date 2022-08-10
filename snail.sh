#!/bin/bash

# @param $1	port
# @param $2	message size
# @param $3 timeout

{
    echo "Start";
    cat -e </dev/urandom | tr -d "\n" | head -c $2;
	echo;
    echo "End";
} > biginbuff.log

timeout ${3}s ./miniserv $1 | timeout ${3}s ./snail ${3} 1024 >/dev/null &
sleep 1;
nc -w $3 localhost $1 >bigoutraw.log & LISTENER=$!
sleep 1;
nc -w 0 localhost $1 <biginbuff.log & EMITTER=$!

wait $LISTENER $EMITTER

