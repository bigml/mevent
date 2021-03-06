#!/bin/bash

PATH=/usr/local/bin:/usr/local/sbin:/bin:/usr/bin:/usr/sbin

SITE_NAME=miad
SITE_PATH=/usr/local/miad/

useage()
{
   echo "useage: $0 -i ipsfile -n -c -b -t -x"
   echo "-i ipsfile: deplopy machines ip list, default is ip.list"
   echo "-n: for fresh system, will deploy system library"
   echo "-c: deploy config file"
   echo "-b: deploy binary file"
   echo "-x: restart binary"
   echo "example: $0 -bx"
   exit -1
}

IPFILE=ip_test_mevent.list
FRESH=0
CONFIG=0
BINARY=0
RESTART=0

DIR_BIN=${SITE_PATH}/mevent/server/daemon/
DIR_SHELL=${SITE_PATH}/mevent/tools
DIR_CFG=${SITE_PATH}/mevent/server/oiad/

BACK_DIR=$(date +%Y%m%d%H%m%S)

# process parameter
while getopts 'i:ncbtx' OPT; do
    case $OPT in
        i)
            IPFILE="$OPTARG";;
        n)
            FRESH=1;;
        c)
            CONFIG=1;;
        b)
            BINARY=1;;
        x)
            RESTART=1;;
        ?)
            useage
    esac
done

for i in `cat $IPFILE`;
do
    echo "deploy to " $i "..."


    if [ $FRESH -eq 1 ]; then
        echo "system library ..."
        rsync -rl /usr/local/lib root@$i:/usr/local/
        ssh root@$i > /dev/null 2>&1 <<EOF
mkdir -p $DIR_BIN
mkdir -p /var/log/moon/${SITE_NAME}/
mkdir -p $DIR_SHELL
mkdir -p /etc/mevent
if ! grep '/usr/local/lib' /etc/ld.so.conf > /dev/null 2>&1
then
    echo "/usr/local/lib" >> /etc/ld.so.conf
    ldconfig
fi
EOF
    fi

    if [ $CONFIG -eq 1 ]; then
        echo "config file ..."
        rsync ${DIR_SHELL}/ip_online_mevent.list root@$i:${DIR_SHELL}/
        rsync ${DIR_SHELL}/deploy_online_mevent.sh root@$i:${DIR_SHELL}/
        rsync ${DIR_SHELL}/release.sh root@$i:${DIR_SHELL}/release.sh

        rsync ${DIR_CFG}/server_test.hdf root@$i:/etc/mevent/server.hdf
        rsync ${DIR_CFG}/server_online.hdf root@$i:/etc/mevent/
        rsync ${DIR_CFG}/server_online_masc.hdf root@$i:/etc/mevent/
        rsync ${DIR_CFG}/server_online_ipb.hdf root@$i:/etc/mevent/
        rsync ${DIR_CFG}/server_online_mqc.hdf root@$i:/etc/mevent/
        rsync ${DIR_CFG}/server_bakstation.hdf root@$i:/etc/mevent/
        rsync ${DIR_CFG}/client_forserver_online.hdf root@$i:/etc/mevent/
    fi

    if [ $BINARY -eq 1 ]; then
        echo "binary ..."
        rsync -rl /usr/local/lib root@$i:/usr/local/
        rsync ${DIR_BIN}mevent root@$i:${DIR_BIN}mevent
        rsync ${DIR_BIN}hb root@$i:${DIR_BIN}hb
        rsync ${DIR_BIN}../test/syscmd root@$i:${DIR_BIN}syscmd
    fi

    if [ $RESTART -eq 1 ]; then
        echo "restart ..."
        ssh root@$i > /dev/null 2>&1 <<EOF
killall mevent
sleep 2
${SITE_PATH}/mevent/server/daemon/mevent -c /etc/mevent/server.hdf >> /tmp/meventstart 2>&1
EOF
    fi

    echo "done"
done
