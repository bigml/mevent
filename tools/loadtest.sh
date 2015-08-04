#!/bin/bash

PATH=/usr/local/bin:/usr/local/sbin:/bin:/usr/bin:/usr/sbin

IPFILE=ip_loadtest.list

CUR_TIME=$(date +"%Y-%m-%d %H:%m:%S")
HEAD_MSG=loadtest

useage()
{
   echo "useage: $0 -m log header message"
   echo "-m message: will prepend to loadtest log file on destnation machine"
   echo "example: $0 -m '1 ad'"
   exit -1
}

while getopts 'm:' OPT; do
    case $OPT in
        m)
            HEAD_MSG="$OPTARG";;
        ?)
            useage
    esac
done


for i in `cat $IPFILE`;
do
    echo "start load test on " $i "..."

    ssh root@$i > /dev/null 2>&1 <<EOF
killall masc
sleep 1
echo -e "\n$CUR_TIME $HEAD_MSG" >> /tmp/mascload.log
/root/masc /root/client_ml_dev.hdf masc 1001 >> /tmp/mascload.log 2>&1 &
EOF
    echo "done"
done
