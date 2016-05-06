#include "sys.h"

#include <stdlib.h>

bool     sys::friendly_exit          = false;
int      sys::svc_launch_time        = 0;
reactor *sys::r                      = NULL;

int sys::init_svc()
{
  time_util::init();
  ::srand(time_value::start_time.sec() + time_value::start_time.usec());

  // check sys config
  if (init_sys_log("log") != 0)
    return -1;

  s_log->rinfo("init %s", g_proc_name);

  // init network module
  inet_address data_src_listen_addr(8801, "0.0.0.0");
  acceptor<src_client> src_acceptor;
  int ret = src_acceptor.open(data_src_listen_addr, sys::r);
  if (ret == -1) {
    fprintf(stderr, "acceptor open failed![%s]\n", strerror(errno));
    return -1;
  }

  inet_address fenfa_listen_addr(8901, "0.0.0.0");
  acceptor<fenfa_client> fenfa_acceptor;
  int ret = fenfa_acceptor.open(fenfa_listen_addr, sys::r);
  if (ret == -1) {
    fprintf(stderr, "acceptor open failed![%s]\n", strerror(errno));
    return -1;
  }

  time_util::now = time_value::gettimeofday().sec();
}
