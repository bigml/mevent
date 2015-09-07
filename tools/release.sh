#!/bin/bash

PATH=/usr/local/bin:/usr/local/sbin:/bin:/usr/bin:/usr/sbin

PACKAGE_NAME=mevent
V_MAJOR=`cat v.major`
V_MINOR=`cat v.minor`
V_REV=`cat v.rev`

# major
# 主版本号，不同主版本号的程序集不可互换。
# 例如，这适用于对产品的大量重写，这些重写使得无法实现向后兼容性。

# minor
# 次版本号，主版本号相同，而次版本号不同，这指示显著增强，但照顾到了向后兼容性。
# 例如，这适用于产品的修正版或完全向后兼容的新版本。

# revision
# 修订号，主版本号和次版本号都相同但修订号不同的程序集应是完全可互换的。
# 主要适用于修复以前发布的程序集中的bug、安全漏洞等。

# build
# 内部版本号的不同表示对相同源所作的重新编译。这适合于更改处理器、平台或编译器的情况。（基本用不到）

useage()
{
   echo "useage: $0 -a -b -c"
   echo "-a: 主版本更新"
   echo "-b: 次版本更新"
   echo "-c: 修订版本更新"
   echo "不提供任何参数时，按照当前的版本，重新打包。"
   echo "example: $0 -c"
   exit -1
}

while getopts 'abc' OPT; do
    case $OPT in
        a)
            V_MAJOR=`expr $V_MAJOR + 1`;
            V_MINOR=0;
            V_REV=0;;
        b)
            V_MINOR=`expr $V_MINOR + 1`;
            V_REV=0;;
        c)
            V_REV=`expr $V_REV + 1`;;
        ?)
            useage
    esac
done

VERSION=${V_MAJOR}.${V_MINOR}.${V_REV}

echo "prepare "$VERSION
echo $VERSION > /usr/local/miad/version

cd /;
sudo mkdir -p /usr/local/release/
sudo mv ${PACKAGE_NAME}.* /usr/local/release/;
sudo tar zcvf ${PACKAGE_NAME}.${VERSION}.tar.gz \
    /etc/mevent/ \
    /usr/local/miad/ \
    /usr/local/lib/libmnl.so \
    /usr/local/lib/libmevent.so \
    /usr/local/lib/libmongo-client.* \
    /usr/local/lib/mevent_*.so;
sudo cp ${PACKAGE_NAME}.${VERSION}.tar.gz /opt/mevent;

cd -;

echo $V_MAJOR > v.major
echo $V_MINOR > v.minor
echo $V_REV > v.rev

echo "done"
