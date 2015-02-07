#include <string.h>
#include <glib-2.0/glib.h>

#include "mevent.h"

HDF *g_cfg = NULL;
HASH *g_datah = NULL;

void get_ott_available_player(ULIST **players)
{
    HASH *player = NULL;

    uListInit(players, 10, 0);

    hash_init(&player, hash_str_hash, hash_str_comp, NULL);

    /* 线上 ott 正式播放器 id 为 4820 */
    ne_hash_insert(player, (void *) strdup("boardid"), (void *) strdup("4820"));

    uListAppend(*players, player);
}

void get_ott_available_users(ULIST **users)
{
    HASH *user = NULL;

    uListInit(users, 10, 0);

    hash_init(&user, hash_str_hash, hash_str_comp, NULL);

    ne_hash_insert(user, (void *) strdup("userid"), (void*) strdup("773032811"));
    ne_hash_insert(user, (void *) strdup("ptype"),  (void*) strdup("100"));

    uListAppend(*users, user);
}

void get_ott_available_videos(ULIST **videos)
{
    HASH *video = NULL;

    uListInit(videos, 10, 0);

    hash_init(&video, hash_str_hash, hash_str_comp, NULL);

    ne_hash_insert(video, (void *) strdup("cid"),      (void *) strdup("27"));
    ne_hash_insert(video, (void *) strdup("hid"),      (void *) strdup("12149"));
    ne_hash_insert(video, (void *) strdup("vid"),      (void *) strdup("489463"));
    ne_hash_insert(video, (void *) strdup("on_year"),  (void *) strdup("2013"));
    ne_hash_insert(video, (void *) strdup("sub_type"), (void *) strdup("剧情,喜剧,家庭,都市,内地剧场"));

    uListAppend(*videos, video);
}

void select_ott_random_player(HASH **player)
{
    ULIST *players = NULL;

    get_ott_available_player(&players);

    uListGet(players, g_random_int_range(0, uListLength(players)), (void **) player);
}

void select_ott_random_user(HASH **user)
{
    ULIST *users = NULL;

    get_ott_available_users(&users);

    uListGet(users, g_random_int_range(0, uListLength(users)), (void **) user);
}

void select_ott_random_video(HASH **video)
{
    ULIST *videos = NULL;

    get_ott_available_videos(&videos);

    uListGet(videos, g_random_int_range(0, uListLength(videos)), (void **) video);
}

void read_player_advertising_wrapper(mevent_t *evt, HASH *player, HASH *user, HASH *video)
{
    char *boardid = (char *) hash_lookup(player, "boardid");
    hdf_set_value(evt->hdfsnd, "boardid", boardid);

    char *userid = (char *) hash_lookup(user, "userid");
    char *ptype  = (char *) hash_lookup(user, "ptype");

    hdf_set_value(evt->hdfsnd, "deviceid", userid);
    hdf_set_value(evt->hdfsnd, "ptype", ptype);

    char *cid       = (char *) hash_lookup(video, "cid");
    char *hid       = (char *) hash_lookup(video, "hid");
    char *vid       = (char *) hash_lookup(video, "vid");
    char *on_year   = (char *) hash_lookup(video, "on_year");
    char *sub_type  = (char *) hash_lookup(video, "sub_type");
    char *clip_type = (char *) hash_lookup(video, "clip_type");

    hdf_set_value(evt->hdfsnd, "cid", cid);
    hdf_set_value(evt->hdfsnd, "hid", hid);
    hdf_set_value(evt->hdfsnd, "videoid", vid);
    hdf_set_value(evt->hdfsnd, "on_year", on_year);
    hdf_set_value(evt->hdfsnd, "sub_type", sub_type);

    if (clip_type == NULL) {
        /* OTT 缺少表示正短片的字段，因此默认填充 1 */
        hdf_set_value(evt->hdfsnd, "clip_type", "1");
    }

    mevent_trigger(evt, NULL, 1001, FLAGS_SYNC);

    HDF  *attachment  = NULL;
    HDF  *spot        = NULL;
    HDF  *destination = evt->hdfrcv;
    char *delivery_id = NULL;

    attachment = hdf_obj_child(destination);
    while (attachment != NULL) {
        spot = hdf_get_child(attachment, "spots");
        while (spot != NULL) {
            delivery_id = hdf_get_value(spot, "delivery.delivery_id", "0");
            g_print("candidate delivery id is %s\n", delivery_id);

            /* FIXME: 触发上报事件会退出程序 */
            /*
             * 由于无法从返回报文中获知投放的流量类型，因此此处假定所有
             * 的广告流量类型为 cpm
             */
            hdf_set_value(evt->hdfsnd, "cardid", delivery_id);
            hdf_set_value(evt->hdfsnd, "type", "1");
            hdf_set_value(evt->hdfsnd, "deviceid", userid);
            mevent_trigger(evt, NULL, 1011, FLAGS_SYNC);

            spot = hdf_obj_next(spot);
        }

        attachment = hdf_obj_next(attachment);
    }
}

void read_ott_player_advertising_unittest(mevent_t *evt)
{
    HASH *selected_player = NULL;
    HASH *selected_user   = NULL;;
    HASH *selected_video  = NULL;

    select_ott_random_player(&selected_player);
    select_ott_random_user(&selected_user);
    select_ott_random_video(&selected_video);

    read_player_advertising_wrapper(
        evt, selected_player, selected_user, selected_video
    );
}

void update_advertising_player_unittest(mevent_t *evt)
{
    mevent_trigger(evt, NULL, 1010, FLAGS_NONE);
}

int main(int argc, char *argv[])
{
    mevent_t *evt;
    char  plugin[64] = "masc";
    char *fname      = NULL;
    int   times      = 1000;

    if (argc > 2) {
        printf("Usage: %s [CONFIG_FILE]\n", argv[0]);

        return 1;
    }

    if (argv[1] == NULL) {
        fname = g_strdup_printf("%s", "/home/ml/miad/xport/client_dev_bc.hdf");
    } else {
        fname = argv[1];
    }

    evt = mevent_init_plugin(plugin, fname);

    for (int i = 0; i < times; i ++) {
        read_ott_player_advertising_unittest(evt);
    }

    mevent_free(evt);

    return 0;
}
