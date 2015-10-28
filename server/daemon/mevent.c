#include <pthread.h>        /* threading functions */
#include <time.h>        /* nanosleep() */
#include <errno.h>        /* ETIMEDOUT */
#include <sys/types.h>        /* for size_t */
#include <stdint.h>        /* for [u]int*_t */
#include <stdlib.h>        /* for malloc() */
#include <string.h>        /* for memcpy()/memcmp() */
#include <stdio.h>        /* snprintf() */
#include <dlfcn.h>
#include "log.h"

#include "net-const.h"
#include "common.h"
#include "mevent.h"
#include "mheads.h"

/*
 * The hash function used is the "One at a time" function, which seems simple,
 * fast and popular. Others for future consideration if speed becomes an issue
 * include:
 *  * FNV Hash (http://www.isthe.com/chongo/tech/comp/fnv/)
 *  * SuperFastHash (http://www.azillionmonkeys.com/qed/hash.html)
 *  * Judy dynamic arrays (http://judy.sf.net)
 *
 * A good comparison can be found at
 * http://eternallyconfuzzled.com/tuts/hashing.html#existing
 */

static uint32_t hash(const unsigned char *key, const size_t ksize)
{
    uint32_t h = 0;
    size_t i;

    for (i = 0; i < ksize; i++) {
        h += key[i];
        h += (h << 10);
        h ^= (h >> 6);
    }
    h += (h << 3);
    h ^= (h >> 11);
    h += (h << 15);
    return h;
}


static pthread_key_t m_pos_key;
static pthread_once_t m_once = PTHREAD_ONCE_INIT;

static void _tls_make_myposkey()
{
    pthread_key_create(&m_pos_key, free);
}

static int* _tls_get_mypos()
{
    void *ptr;

    pthread_once(&m_once, _tls_make_myposkey);

    if ((ptr = pthread_getspecific(m_pos_key)) == NULL) {
        ptr = malloc(sizeof(int));
        pthread_setspecific(m_pos_key, ptr);
    }

    return (int*) ptr;
}

static void* mevent_start_base_entry(void *arg)
{
    int rv;
    struct timespec ts;
    struct queue_entry *q;

    struct event_entry *e = (struct event_entry*)arg;
    int *mypos = _tls_get_mypos();
    *mypos = e->cur_thread;

    mtc_dbg("I'm %s %dnd thread", e->name, *mypos);

    for (;;) {
        /* Condition waits are specified with absolute timeouts, see
         * pthread_cond_timedwait()'s SUSv3 specification for more
         * information. We need to calculate it each time.
         * We sleep for 1 sec. There's no real need for it to be too
         * fast (it's only used so that stop detection doesn't take
         * long), but we don't want it to be too slow either. */
        mutil_utc_time(&ts);
        ts.tv_sec += 1;

        rv = 0;
        queue_lock(e->op_queue[*mypos]);
        while (queue_isempty(e->op_queue[*mypos]) && rv == 0) {
            rv = queue_timedwait(e->op_queue[*mypos], &ts);
        }

        if (rv != 0 && rv != ETIMEDOUT) {
            errlog("Error in queue_timedwait()");
            /* When the timedwait fails the lock is released, so
             * we need to properly annotate this case. */
            __release(e->op_queue[*mypos]->lock);
            continue;
        }

        q = queue_get(e->op_queue[*mypos]);
        queue_unlock(e->op_queue[*mypos]);

        if (q == NULL) {
            if (e->loop_should_stop) {
                break;
            } else {
                continue;
            }
        }

        mtc_dbg("%s %d process", e->name, *mypos);

        e->process_driver(e, q, *mypos);

        /* Free the entry that was allocated when tipc queued the
         * operation. This also frees it's components. */
        queue_entry_free(q);
    }

    return NULL;
}

static void mevent_stop_driver(struct event_entry *e)
{
    if (e == NULL) return;

    //dlclose(e->lib);
    e->loop_should_stop = 1;
    e->stop_driver(e);

    for (int i = 0; i < e->num_thread; i++) {
        pthread_join(*(e->op_thread[i]), NULL);
        free(e->op_thread[i]);
        e->op_thread[i] = NULL;
        queue_free(e->op_queue[i]);
    }

    for (int i = 0; i < MAX_THREAD_NUM && e->mt_thread[i] != NULL; i++) {
        pthread_join(*(e->mt_thread[i]), NULL);
        free(e->mt_thread[i]);
        e->mt_thread[i] = NULL;
    }

    if (e->name != NULL) free(e->name);
    free(e);
}

static int mevent_start_driver(struct mevent *evt, struct event_driver *d,
                               void *lib, int num_thread, HDF *matenode)
{
    if (evt == NULL || evt->table == NULL || d == NULL) return 0;

    struct event_entry *e = d->init_driver();
    if (e == NULL) return 0;

    num_thread = num_thread < 0 ? 1 : num_thread;
    if (num_thread > MAX_THREAD_NUM) {
        wlog("plugin %s thread number %d too large, set to default max number %d",
             e->name, num_thread, MAX_THREAD_NUM);
        mtc_warn("plugin %s thread number %d too large, set to default max number %d",
                 e->name, num_thread, MAX_THREAD_NUM);
        num_thread = MAX_THREAD_NUM;
    }

    e->num_thread = num_thread;
    e->cur_thread = 0;

    for (int i = 0; i < num_thread; i++) {
        e->op_queue[i] = queue_create();
        e->op_thread[i] = malloc(sizeof(pthread_t));
        pthread_create(e->op_thread[i], NULL, mevent_start_base_entry, (void*)e);
        sleep(1);
        e->cur_thread++;
    }

    e->cur_thread = 0;

    HDF *cnode = hdf_obj_child(matenode);
    char *dle;
    for (int i = 0; cnode && i < MAX_THREAD_NUM; cnode = hdf_obj_next(cnode)) {
        void* (*mate_func)(void *arg);

        mate_func = dlsym(lib, hdf_obj_value(cnode));
        if ((dle = dlerror()) != NULL) {
            wlog("unable to find %s %s", hdf_obj_value(cnode), dle);
            mtc_err("unable to find %s %s", hdf_obj_value(cnode), dle);
            continue;
        }

        e->mt_thread[i] = malloc(sizeof(pthread_t));
        pthread_create(e->mt_thread[i], NULL, mate_func, (void*)e);
        i++;
    }

    uint32_t h;
    struct event_chain *c;
    h = hash(e->name, e->ksize) % evt->hashlen;
    c = evt->table + h;

    if (c->len == 0) {
        c->first = e;
        c->last = e;
        c->len = 1;
    } else if (c->len <= evt->chainlen) {
        e->next = c->first;
        c->first->prev = e;
        c->first = e;
        c->len += 1;
    } else {
        wlog("unbelieveable, you have more than %d plugins, stop it %s",
             evt->chainlen, e->name);
        mtc_warn("unbelieveable, you have more than %ld plugins, stop it %s",
                 evt->chainlen, e->name);

        mevent_stop_driver(e);
        return 0;
    }

    return 1;
}

/* Looks up the given key in the chain. Returns NULL if not found, or a
 * pointer to the mevent entry if it is. The chain can be empty. */
static struct event_entry *find_in_chain(struct event_chain *c,
                     const unsigned char *key, size_t ksize)
{
    struct event_entry *e;

    for (e = c->first; e != NULL; e = e->next) {
        if (ksize != e->ksize) {
            continue;
        }
        if (memcmp(key, e->name, ksize) == 0) {
            break;
        }
    }

    /* e will be either the found chain or NULL */
    return e;
}



struct mevent* mevent_start(void)
{
    int ret;

    struct mevent *evt = calloc(1, sizeof(struct mevent));
    if (evt == NULL) return NULL;

    evt->numevts = 0;
    evt->chainlen = 1000000;
    //evt->hashlen = evt->numevts / evt->chainlen;
    evt->hashlen = 16;
    evt->table = calloc(evt->hashlen, sizeof(struct event_chain));

    void *lib;
    char tbuf[1024], *tp;
    char *name;
    int num_thread;
    struct event_driver *driver;
    HDF *res = hdf_get_child(g_cfg, PRE_SERVER".plugins");
    char *plugin_path = settings.plugin_path ? settings.plugin_path : PLUGIN_PATH;
    while (res != NULL) {
        lib = NULL; driver = NULL; memset(tbuf, 0x0, sizeof(tbuf));
        name = hdf_get_value(res, "name", "_unexist");
        num_thread = hdf_get_int_value(res, "numberofthreads", 1);
        HDF *mates = hdf_get_obj(res, "mates");

        snprintf(tbuf, sizeof(tbuf), "%s/mevent_%s.so", plugin_path, name);
        //lib = dlopen(tbuf, RTLD_NOW|RTLD_GLOBAL);
        lib = dlopen(tbuf, RTLD_LAZY|RTLD_GLOBAL);
        if (lib == NULL) {
            wlog("open driver %s failure %s\n", tbuf, dlerror());
            res = hdf_obj_next(res);
            continue;
        }

        snprintf(tbuf, sizeof(tbuf), "%s_driver", name);
        driver = (struct event_driver*)dlsym(lib, tbuf);
        if ((tp = dlerror()) != NULL) {
            wlog("find symbol %s failure %s\n", tbuf, tp);
            res = hdf_obj_next(res);
            continue;
        }

        ret = mevent_start_driver(evt, driver, lib, num_thread, mates);
        if (ret != 1) wlog("init driver %s failure\n", name);
        else evt->numevts++;

        res = hdf_obj_next(res);
    }

    return evt;
}

void mevent_stop(struct mevent *evt)
{
    size_t i;
    struct event_chain *c;
    struct event_entry *e, *n;

    if (evt == NULL) return;
    for (i = 0; i < evt->hashlen; i++) {
        c = evt->table + i;
        if (c->first == NULL)
            continue;

        e = c->first;
        while (e != NULL) {
            n = e->next;
            mevent_stop_driver(e);
            e = n;
        }
    }

    free(evt->table);
    free(evt);
}

void mevent_add_timer(struct timer_entry **timers, int timeout, bool repeat,
                      void (*timer)(struct event_entry *e, unsigned int upsec, void *data),
                      void *data)
{
    if (!timers || !timer) return;

    struct timer_entry *t = calloc(1, sizeof(struct timer_entry));
    if (t) {
        t->timeout = timeout;
        t->repeat = repeat;
        t->timer = timer;
        t->next = *timers;
        t->data = data;
        *timers = t;
    }
}

struct event_entry* find_entry_in_table(struct mevent *evt, const unsigned char *key, size_t ksize)
{
    uint32_t h;
    struct event_chain *c;
    h = hash(key, ksize) % evt->hashlen;
    c = evt->table + h;

    return find_in_chain(c, key, ksize);
}
