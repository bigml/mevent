/*
 * 事件中心心跳检测重启程序, 需配合 crontab 定时拉起使用
 * 程序本身得错误信息printf，后台的状态输出到日志文件
 * PATH/hb /etc/mevent/client.hdf uic 1001 > /tmp/meventhb.log
 *
 * 正常情况下 输出 后端插件的返回信息
 */
#include <stdio.h>        /* printf() */
#include <unistd.h>        /* malloc(), fork() and getopt() */
#include <stdlib.h>        /* atoi() */
#include <sys/types.h>    /* for pid_t */
#include <string.h>        /* for strcpy() and strlen() */
#include <pthread.h>    /* for pthread_t */

#include "mevent.h"        /* api's mevent.h */

#include "common.h"
#include "net.h"
#include "net-const.h"
#include "log.h"
#include "stats.h"
#include "config.h"

#include "mheads.h"

#include "ClearSilver.h"

void set_param(HDF *node)
{
    if (!node) return;

    hdf_set_value(node, "boardid", "4820");
    hdf_set_value(node, "ptype", "100");
    hdf_set_value(node, "videoid", "1116403");
    hdf_set_value(node, "vtt", "1200");
    hdf_set_value(node, "posid", "1156130200");

    hdf_set_value(node, "deviceid", "mgtvmac0066CE02E7EC");
    hdf_set_value(node, "ip", "61.187.53.134");

    hdf_set_value(node, "v.id", "1116403");
    hdf_set_value(node, "v.classification", "电影");
    hdf_set_value(node, "v.collection", "约会之夜");
    hdf_set_value(node, "v.type", "喜剧，犯罪");
    hdf_set_value(node, "v.year", "2010");
    hdf_set_value(node, "v.pianyuan", "1");

    hdf_set_value(node, "m.xx", "yy");
}

int main(int argc, char *argv[])
{
    STRING serror; string_init(&serror);
    int ret;
    char *ename, *conf;
    int cmd;
    int trynum = 6;

    if (argc != 4) {
        printf("Usage: %s CLIENT_CONFIG_FILE EVENT_NAME CMD\n", argv[0]);
        return 1;
    }

    conf = argv[1];
    ename = argv[2];
    cmd = atoi(argv[3]);

    mtc_init("/tmp/meventhb", 7);
    nerr_init();
    merr_init((MeventLog)mtc_msg);

    settings.smsalarm = 0;

    mevent_t *evt = mevent_init_plugin(ename, conf);
    if (evt == NULL) {
        printf("init error\n");
        return 1;
    }

    set_param(evt->hdfsnd);

    ret = mevent_trigger(evt, NULL, cmd, FLAGS_SYNC);
    if (PROCESS_OK(ret)) {
        mtc_foo("ok");
        hdf_dump(evt->hdfrcv, NULL);
    } else {
        int tried = 1;

    redo:
        set_param(evt->hdfsnd);

        string_appendf(&serror, "%d => %d; ", tried, ret);
        sleep(10);
        ret = mevent_trigger(evt, NULL, cmd, FLAGS_SYNC);
        if (PROCESS_NOK(ret) && tried < trynum) {
            tried++;
            goto redo;
        }

        if (PROCESS_NOK(ret) && tried >= trynum) {
            mtc_foo("total  error: %s, restart", serror.buf);
            system("killall -9 mevent; sleep 2; ulimit -S -c 9999999999 && /usr/local/miad/mevent/server/daemon/mevent -c /etc/mevent/server.hdf");
        } else {
            mtc_foo("partly error: %s", serror.buf);
            hdf_dump(evt->hdfrcv, NULL);
        }
    }

    string_clear(&serror);
    mevent_free(evt);
    return 0;
}
