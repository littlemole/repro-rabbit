#!/bin/bash

cd /usr/local/src/ 

git clone https://github.com/CopernicaMarketingSoftware/AMQP-CPP.git 

cd AMQP-CPP  

if [ "$CXX" == "g++" ]; then
    make 
    make install
else
    make CPP=$CXX \
        LD_FLAGS="-shared -stdlib=libc++ -fcoroutines-ts  -lc++abi -std=c++17 -fpic" \
        CPPFLAGS="-c -I../include -std=c++17 -MD -stdlib=libc++ -fcoroutines-ts -fpic" \
        LD=$CXX 
fi
    
make install
