#include "reactor.h"
#include "process.h"
#include "svc_handler.h"
#include "timer_handler.h"
#include "signal_handler.h"
#include "time_value.h"
#include "ilog.h"
#include "sys.h"

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

const char *g_proc_name = "fenfad";
char **os_argv = NULL;

void print_usage()
{
  printf("\033[0;32m\nUsage: %s [OPTION...]\n\n", g_proc_name);
  printf("  -g                    Guard mode\n");
  printf("  -u  user name         Run as user\n");
  printf("  -C                    Core dump enable\n");
  printf("  -p  platform name     \n"); 
  printf("  -s  service sn        \n");
  printf("  -v                    Output version info\n");
  printf("  -h                    Help info\n");
  printf("\n");
  printf("\033[0m\n");
  ::exit(0);
}

int main(int argc, char *argv[])
{ 
  if (argc == 1)
    print_usage();

  char *run_as_user = NULL;
  int dump_core = 0;

  // parse args
  int c = -1;
  extern char *optarg;
  extern int optopt;

  const char *opt = ":gvu:Cs:ep:h";
  while((c = ::getopt(argc, argv, opt)) != -1) {
    switch (c) {
    case 'v':        // version
      fprintf(stderr, "\033[0;32m%s %s\033[0m\n", SVC_EDITION, BIN_V);
      return 0;
    case 'u':
      run_as_user = optarg;
      break;
    case 'C':
      dump_core = 1;
      break;
    case ':':
      fprintf(stderr, 
              "\n\033[0;35mOption -%c requires an operand\033[0m\n",
              optopt);
    case 'h':
    default:
      print_usage();
    }
  }

  if (process::set_max_fds(8192) == -1) { // must first
    fprintf(stderr, "Error: set max fds failed!\n");
    return 1;
  }

  if (run_as_user && process::runas(run_as_user) == -1) {
    fprintf(stderr, "Error: run as user[%s] failed!\n", run_as_user);
    return 1;
  }

  if (dump_core && process::dump_corefile() == -1) {
    fprintf(stderr, "Error: dump corefile failed!\n");
    return 1;
  }

  if (process::child_pid() != 0) { // master process
    os_argv = process::save_argv(argc, argv);
    char argv_bf[256] = {0};
    ::snprintf(argv_bf, sizeof(argv_bf), "%s: guard process ", g_proc_name);
    for (int i = 0; os_argv[i]; ++i) {
      ::strcat(argv_bf, os_argv[i]);
      ::strcat(argv_bf, " ");
    }
    process::set_proc_title(argv, argv_bf);
    if (process::daemon() == -1) {
      fprintf(stderr, "Error: daemon failed!\n");
      return 1;
    }
    char pid_bf[256] = {0};
    ::snprintf(pid_bf, sizeof(pid_bf), "/tmp/%s.pid", g_proc_name);
    process::output_pid(pid_bf);

    process::to_guard_mode("guard.log");
  }

  // child process
  char argv_bf[256] = {0};
  ::snprintf(argv_bf, sizeof(argv_bf), "%s: child process", g_proc_name);
  process::set_proc_title(argv, argv_bf);

  sys::r = new reactor();
  if (sys::r->open() != 0) {
    fprintf(stderr, "Error: reactor - open failed!\n");
    return -1;
  }

  if (sys::init_svc() != 0) {
    fprintf(stderr, "Error: init_svc - init failed!\n");
    return 1;
  }

  sys::svc_launch_time = time_value::gettimeofday().sec();

  s_log->rinfo("arg0 %s os argv = %s", argv[0], new_argv[0]);
}

