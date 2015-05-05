#include "mheads.h"

#include "mevent.h"

HDF *g_cfg = NULL;
HASH *g_datah = NULL;

static void _set_params(HDF *node)
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
}

int main(int argc, char *argv[])
{
    mevent_t *evt;
    int ret, count_ok, count_fai, count_total;
    char plugin[64] = "uic";
    int cmd;

    if (argc <= 3) {
        printf("Usage: %s [CONFIG_FILE] [PLUGIN] [COMMAND]\n", argv[0]);
        return 1;
    }

    strncpy(plugin, argv[2], sizeof(plugin));
    cmd = atoi(argv[3]);

    evt = mevent_init_plugin(plugin, argv[1]);
    if (!evt) {
        printf("init %s failure\n", plugin);
        return 1;
    }

    mtc_init("masc", 7);

    count_ok = count_fai = count_total = 0;

    mtimer_start();
    for (int i = 0; i < 100000; i++) {
        _set_params(evt->hdfsnd);

        ret = mevent_trigger(evt, NULL, cmd, FLAGS_SYNC);
        if (PROCESS_OK(ret)) {
            //printf("process success %d\n", ret);
            //hdf_dump(evt->hdfrcv, NULL);
            count_ok++;
        } else {
            //printf("process failure %d!\n", ret);
            count_fai++;
        }
    }
    count_total = count_ok + count_fai;
    mtimer_stop(NULL);

    printf("total %d, success %d, failure %d, %d per second\n",
           count_total, count_ok, count_fai,
           (int)((count_total * 1.0 / elapsed) * 1000000));

    mevent_free(evt);

    return 0;
}
