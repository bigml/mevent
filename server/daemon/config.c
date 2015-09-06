
#include <stdio.h>        /* vsprintf() */
#include <stdarg.h>
#include <sys/types.h>         /* open() */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>         /* write() */
#include <string.h>        /* strcmp(), strerror() */
#include <errno.h>        /* errno */
#include <time.h>        /* time() and friends */

#include "log.h"
#include "common.h"
#include "mheads.h"
#include "ClearSilver.h"
#include "config.h"

int config_parse_file(const char *file, HDF **cfg)
{
    bool readok = true;
    NEOERR *err;

    err = hdf_init(cfg);
    RETURN_V_NOK(err, 0);

    err = hdf_read_file(*cfg, file);
    if (err != STATUS_OK) readok = false;
    OUTPUT_NOK(err);

    if (!readok) return 0;

    return 1;
}

void config_cleanup(HDF **config)
{
    if (*config == NULL) return;
    hdf_destroy(config);
}

void config_reload_trace_level(int fd, short event, void *arg)
{
    HDF *node;

    hdf_init(&node);
    hdf_read_file(node, settings.conffname);

    int level = hdf_get_int_value(node, PRE_CONFIG".trace_level", TC_DEFAULT_LEVEL);

    mtc_foo("set trace level to %d", level);

    mtc_set_level(level);

    hdf_destroy(&node);
}
