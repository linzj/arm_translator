#!/bin/sh
valgrind --tool=callgrind  --callgrind-out-file=call.out ./out/Debug/testQEMU $1
if ! [ -f call.out ]
    then
        echo "not callgrind output" >2
fi
for func in `cat symbols_to_be_tested`
    do
        if  grep $func call.out >/dev/null 2>/dev/null
            then
                sed -i "/$func/d" symbols_to_be_tested
                echo "Removed symbol $func"
        fi
done
rm call.out
