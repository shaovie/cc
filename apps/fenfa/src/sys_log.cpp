#include "sys_log.h"

// Lib header
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

int init_sys_log(const char *log_cfg_dir)
{
  if (::mkdir(log_cfg_dir, 0644) == -1 && errno != EEXIST)
    return -1;

  //= init log
  char bf[128] = {0};
  ::snprintf(bf, sizeof(bf), "%s/sys_log.conf", log_cfg_dir);
  if (sys_log::instance()->init(bf) != 0)
  {
    fprintf(stderr, "Error: ilog - init sys log failed!\n");
    return -1;
  }
  //
  ::snprintf(bf, sizeof(bf), "%s/err_log.conf", log_cfg_dir);
  if (err_log::instance()->init(bf) != 0)
  {
    fprintf(stderr, "Error: ilog - init err log failed!\n");
    return -1;
  }
  return 0;
}
