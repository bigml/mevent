#include <stdbool.h>
#include <event.h>

#include "cache.h"
#include "common.h"

/* Define the common structures that are used throughout the whole server. */
struct settings settings;
struct stats stats;
struct mevent *mevent;
HDF *g_cfg;

void clock_handler(const int fd, const short which, void *arg)
{
    struct event *clock_evt = (struct event*) arg;
    struct timeval t = {.tv_sec = 0, .tv_usec = 100000};
    static bool initialized = false;
    static int upsec = 0;
    bool stepsec = false;

    if (initialized) {
        /* only delete the event if it's actually there. */
        evtimer_del(clock_evt);
    } else {
        initialized = true;
    }

    evtimer_set(clock_evt, clock_handler, clock_evt);
    evtimer_add(clock_evt, &t);

    g_ctimef = ne_timef();
    if ((time_t)g_ctimef - g_ctime >= 1) {
        upsec += (time_t)g_ctimef - g_ctime;
        stepsec = true;
    }
    g_ctime = (time_t) g_ctimef;

    /*
     * don't call callback multi time in one second
     */
    if (!stepsec) return;

    struct event_chain *c;
    struct event_entry *e;
    for (size_t i = 0; i < mevent->hashlen; i++) {
        c = mevent->table + i;

        e = c->first;
        while (e) {
            struct timer_entry *t = e->timers, *p, *n;
            p = n = t;
            while (t && t->timeout > 0) {
                if (upsec % t->timeout == 0) {
                    t->timer(e, upsec, t->data);
                    if (!t->repeat) {
                        /*
                         * drop this timer, for cpu useage
                         */
                        if (t == e->timers) e->timers = t->next;
                        else p->next = t->next;

                        n = t->next;
                        free(t);
                        t = n;
                        continue;
                    }
                }
                p = t;
                t = t->next;
            }
            e = e->next;
        }
    }
}

// Explode a string in an array.
size_t explode(const char split, char *input, char **tP, unsigned int limit)
{
    size_t i = 0;

    tP[0] = input;
    for (i = 0; *input; input++) {
        if (*input == split) {
            i++;
            *input = '\0';
            if(*(input + 1) != '\0' && *(input + 1) != split) {
                tP[i] = input + 1;
            } else {
                i--;
            }
        }
        if ((i+1) == limit) {
            return i;
        }
    }

    return i;
}
