#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "mevent.h"

HDF *g_cfg = NULL;
HASH *g_datah = NULL;

int main(int argc, char *argv[])
{
    mevent_t *evt;
    int ret;
    char plugin[64] = "uic";
    int cmd;
    char key[64], val[64];

    cmd = 100;

    if (argc > 2) {
        strncpy(plugin, argv[2], sizeof(plugin));
    } else {
        printf("Usage: %s [CONFIG_FILE] [PLUGIN] [COMMAND] [key] [val]\n", argv[0]);
        return 1;
    }
    if (argc > 2) {
        cmd = atoi(argv[3]);
    }
    if (argc > 3) {
        strncpy(key, argv[4], sizeof(key));
    }
    if (argc > 4) {
        strncpy(val, argv[5], sizeof(val));
    }

    evt = mevent_init_plugin(plugin, argv[1]);

    hdf_set_value(evt->hdfsnd, "boardid", "27");
    hdf_set_value(evt->hdfsnd, "ptype", "1");
    hdf_set_value(evt->hdfsnd, "cid", "1");
    hdf_set_value(evt->hdfsnd, "hid", "1");
    hdf_set_value(evt->hdfsnd, "videoid", "1");
    hdf_set_value(evt->hdfsnd, "sub_type", "1");
    hdf_set_value(evt->hdfsnd, "clip_type", "1");
    hdf_set_value(evt->hdfsnd, "deviceid", "1");
    hdf_set_value(evt->hdfsnd, "on_year", "1");
    hdf_set_value(evt->hdfsnd, "keyword", "1");

    hdf_set_value(evt->hdfsnd, "userid", "2");
    hdf_set_value(evt->hdfsnd, "ptype", "3");
    hdf_set_value(evt->hdfsnd, "cid", "333");
    hdf_set_value(evt->hdfsnd, "hid", "888");
    hdf_set_value(evt->hdfsnd, "vid", "3332");

    ret = mevent_trigger(evt, NULL, cmd, FLAGS_SYNC);
    if (PROCESS_OK(ret)) {
        printf("process success %d\n", ret);
        hdf_dump(evt->hdfrcv, NULL);
    } else {
        printf("process failure %d!\n", ret);
    }

    mevent_free(evt);
    return 0;
}
