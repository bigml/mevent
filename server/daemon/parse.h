
#ifndef _PARSE_H
#define _PARSE_H

#include "req.h"

#include "mheads.h"

/* 变宽域 http://www.cplusplus.com/reference/cstdio/printf/ */
#ifdef DEBUG_MSG
#define MSG_DUMP(pre, p, psize)                                         \
    do {                                                                \
        mtc_dbg("%s%.*s", pre, psize, p);                               \
    } while (0)
#else
#define MSG_DUMP(pre, p, psize)
#endif

int parse_message(struct req_info *req,
          const unsigned char *buf, size_t len);

#endif
