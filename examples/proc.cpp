#include "process.h"
#include "singleton.h"
#include "ilog.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

class sys_log : public singleton<sys_log>, public ilog
{
  friend class singleton<sys_log>;
private:
  sys_log() {};
};

class err_log : public singleton<err_log>, public ilog
{
  friend class singleton<err_log>;
private:
  err_log() {};
};

static ilog_obj *e_log = err_log::instance()->get_ilog("base");
static ilog_obj *s_log = sys_log::instance()->get_ilog("base");

#define LOG_CFG_DIR    "cfg"
int init_sys_log(const char *svc_name)
{
  //= init log
  char bf[128] = {0};
  ::snprintf(bf, sizeof(bf), "%s/sys_log.%s", LOG_CFG_DIR, svc_name);
  if (sys_log::instance()->init(bf) != 0) {
    fprintf(stderr, "Error: ilog - init sys log failed!\n");
    return -1;
  }
  //
  ::snprintf(bf, sizeof(bf), "%s/err_log.%s", LOG_CFG_DIR, svc_name);
  if (err_log::instance()->init(bf) != 0) {
    fprintf(stderr, "Error: ilog - init err log failed!\n");
    return -1;
  }
  return 0;
}

char **new_argv = NULL;
int main(int argc, char *argv[])
{
  if (process::child_pid() != 0) {
    if (process::set_max_fds(81921) == -1) { // must first
      fprintf(stderr, "Error: set max fds failed!\n");
      return 1;
    }

    if (process::runas("shaowei") == -1) {
      fprintf(stderr, "Error: run as user failed!\n");
      return 1;
    }

    if (process::dump_corefile() == -1) {
      fprintf(stderr, "Error: dump corefile failed!\n");
      return 1;
    }

    new_argv = process::save_argv(argc, argv);
    char argv_bf[256] = {0};
    ::strcat(argv_bf, "cui: master ");
    for (int i = 0; new_argv[i]; ++i) {
      ::strcat(argv_bf, new_argv[i]);
      ::strcat(argv_bf, " ");
    }
    process::set_proc_title(argv, argv_bf);

    if (process::daemon() == -1) {
      fprintf(stderr, "Error: daemon failed!\n");
      return 1;
    }
    process::output_pid("/tmp/proc.pid");

    process::to_guard_mode("xxxx.log");
  }

  init_sys_log("t");

  char argv_bf[256] = {0};
  ::strcat(argv_bf, "cui: child ");
  for (int i = 0; new_argv[i]; ++i) {
    ::strcat(argv_bf, new_argv[i]);
    ::strcat(argv_bf, " ");
  }
  process::set_proc_title(argv, argv_bf);

  s_log->rinfo("arg0 %s os argv = %s", argv[0], new_argv[0]);

  ::sleep(10000);

  return 2;
}
