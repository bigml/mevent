#include "mevent_plugin.h"
#include "mevent_skeleton.h"
#include "skeleton_pri.h"

static void skeleton_process_driver(EventEntry *entry, QueueEntry *q)
{
    struct skeleton_entry *e = (struct skeleton_entry*)entry;
    NEOERR *err = NULL;
    HDF *node;
    int ret;

    struct skeleton_stats *st = &(e->st);

    st->msg_total++;

    mtc_dbg("process cmd %u", q->operation);
    switch (q->operation) {
        CASE_SYS_CMD(q->operation, q, e->cd, err);
    case REQ_CMD_STATS:
        st->msg_stats++;
        err = STATUS_OK;
        mcs_set_int64_value(q->hdfsnd, "msg_total", st->msg_total);
        mcs_set_int64_value(q->hdfsnd, "msg_unrec", st->msg_unrec);
        mcs_set_int64_value(q->hdfsnd, "msg_badparam", st->msg_badparam);
        mcs_set_int64_value(q->hdfsnd, "msg_stats", st->msg_stats);
        mcs_set_int64_value(q->hdfsnd, "proc_suc", st->proc_suc);
        mcs_set_int64_value(q->hdfsnd, "proc_fai", st->proc_fai);
        for (int i = 0; i < entry->num_thread; i++) {
            node = mcs_fetch_nodef(q->hdfsnd, "thread.%d", i);
            mcs_set_int64_value(node, "queue_size", entry->op_queue[i]->size);
        }
        break;
    default:
        st->msg_unrec++;
        err = nerr_raise(REP_ERR_UNKREQ, "unknown command %u", q->operation);
        break;
    }

    NEOERR *neede = mcs_err_valid(err);
    ret = neede ? neede->error : REP_OK;
    if (PROCESS_OK(ret)) {
        st->proc_suc++;
    } else {
        st->proc_fai++;
        if (ret == REP_ERR_BADPARAM) {
            st->msg_badparam++;
        }
        TRACE_ERR(q, ret, err);
    }
    if (q->req->flags & FLAGS_SYNC) {
            reply_trigger(q, ret);
    }
}

static void skeleton_stop_driver(EventEntry *entry)
{
    struct skeleton_entry *e = (struct skeleton_entry*)entry;

    /*
     * e->base.name, e->base will free by mevent_stop_driver()
     */
    mdb_destroy(e->db);
    cache_free(e->cd);
}



static EventEntry* skeleton_init_driver(void)
{
    struct skeleton_entry *e = calloc(1, sizeof(struct skeleton_entry));
    if (e == NULL) return NULL;
    NEOERR *err;

    e->base.name = (unsigned char*)strdup(PLUGIN_NAME);
    e->base.ksize = strlen(PLUGIN_NAME);
    e->base.process_driver = skeleton_process_driver;
    e->base.stop_driver = skeleton_stop_driver;
    //mevent_add_timer(&e->base.timers, 60, true, hint_timer_up_term);

    //char *s = hdf_get_value(g_cfg, CONFIG_PATH".dbsn", NULL);
    //err = mdb_init(&e->db, s);
    //JUMP_NOK(err, error);

    e->cd = cache_create(hdf_get_int_value(g_cfg, CONFIG_PATH".numobjs", 1024), 0);
    if (e->cd == NULL) {
        wlog("init cache failure");
        goto error;
    }

    return (EventEntry*)e;

error:
    if (e->base.name) free(e->base.name);
    if (e->db) mdb_destroy(e->db);
    if (e->cd) cache_free(e->cd);
    free(e);
    return NULL;
}

struct event_driver skeleton_driver = {
    .name = (unsigned char*)PLUGIN_NAME,
    .init_driver = skeleton_init_driver,
};
