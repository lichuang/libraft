#!/bin/sh

#!/bin/sh

pwd=`pwd`
third_party=${pwd}/third_party
rm -fr ${third_party}/*

protobuf=protobuf-3.14.0
gtest=googletest-release-1.8.0
gflags=gflags-2.2.2
libevent=libevent-2.1.12-stable

mkdir ${third_party}/include -p
mkdir ${third_party}/lib -p

cd $pwd/deps
echo "compile ${gflags}..."
rm -fr ${gflags}
tar xvf ${gflags}.tar.gz
cd ${gflags}
cmake .
make -j8
cp -r include/gflags ${third_party}/include/
cp -r lib/* ${third_party}/lib/
cd ../
rm -fr ${gflags}
echo "compile ${gflags} done"

cd $pwd/deps
echo "compile ${gtest}..."
rm -fr ${gtest}
tar xvf ${gtest}.tar.gz
cd ${gtest}
cmake .
make -j8
cp ./googlemock/libgmock* ${third_party}/lib/
cp ./googlemock/gtest/libgtest* ${third_party}/lib/
cp -r googlemock/include/gmock ${third_party}/include/
cp -r googletest/include/gtest ${third_party}/include/
cd ../
rm -fr ${gtest}
echo "compile ${gtest} done"

cd $pwd/deps
echo "compile ${protobuf}..."
rm -fr ${protobuf}
tar xvf ${protobuf}.tar.gz
cd ${protobuf}
./autogen.sh
./configure --prefix=${third_party}
make -j6
make install
cd ..
rm -fr ${protobuf}
echo "compile ${protobuf} done"

cd ${pwd}

cd $pwd/deps
echo "compile ${libevent}..."
rm -fr ${libevent}
tar xvf ${libevent}.tar.gz
cd ${libevent}
./autogen.sh
./configure --prefix=${third_party} --disable-openssl
make -j6
make install
cd ..
rm -fr ${libevent}
echo "compile ${libevent} done"

cd ${pwd}
