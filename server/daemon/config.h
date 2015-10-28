#ifndef __MCONFIG_H__
#define __MCONFIG_H__

int config_parse_file(const char *file, HDF **cfg);
void config_cleanup(HDF **config);
void config_reload_trace_level(int fd, short event, void *arg);

#endif    /* __MCONFIG_H__ */
