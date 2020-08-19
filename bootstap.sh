#!/bin/sh

pwd=`pwd`
third_party=${pwd}/third_party
libevent=libevent-2.1.12-stable

mkdir ${third_party}/include -p
mkdir ${third_party}/lib -p

cd $pwd/deps
echo "compile ${libevent}..."
rm -fr ${libevent}
tar xvf ${libevent}.tar.gz
cd ${libevent}
./autogen.sh
./configure --prefix=${third_party}
make -j6
make install
cd ..
rm -fr ${libevent}
echo "compile ${libevent} done"

cd ${pwd}
