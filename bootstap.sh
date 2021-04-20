#!/bin/sh

#!/bin/sh

pwd=`pwd`
third_party=${pwd}/third_party
rm -fr ${third_party}
mkdir -p ${third_party}

protobuf=protobuf-3.14.0
gtest=googletest-release-1.8.0

mkdir -p ${third_party}/include
mkdir -p ${third_party}/lib
mkdir -p ${third_party}/bin

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
