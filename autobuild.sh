#!/bin/bash

set -e

#清理旧的build目录后重新创建
rm -rf `pwd`/build
mkdir -p `pwd`/build

cd `pwd`/build &&
    cmake .. &&
    make

#回到项目根目录
cd ..

#把头文件拷贝到 /usr/include/mymuduo so库拷贝到 /usr/lib  PATH
if [ ! -d /usr/include/mymuduo ]; then
    mkdir /usr/include/mymuduo
fi

for header in *.h
do
    cp $header /usr/include/mymuduo
done

cp `pwd`/lib/libmymuduo.so /usr/lib

ldconfig