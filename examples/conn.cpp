#include "reactor.h"
#include "async_reactor_notify.h"
#include "svc_handler.h"
#include "timer_handler.h"
#include "signal_handler.h"
#include "acceptor.h"
#include "connector.h"
#include "mblock.h"
#include "guard.h"
#include "task.h"
#include "thread_mutex.h"
#include "ilog.h"
#include "ilist.h"
#include "singleton.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <cassert>
#include <signal.h>
#include <cctype>
#include <sched.h>

char *remote_addr = NULL;
class client;
connector<client> i_connector;

class client : public svc_handler
{
public:
  client(const int port) :
    port_(port)
  {
  }

  virtual int connect_timeout()
  {
    //fprintf(stderr, "connect %d timeout\n", this->port_);
    this->handle_close(ev_handler::connect_mask);
    return 0;
  }

  virtual int open(ev_handler *)
  {
    fprintf(stderr, "connect %d ok \n", this->port_);
    return -1;
  }

  virtual int handle_close(reactor_mask mask)
  {
    if (mask == ev_handler::write_mask)
      fprintf(stderr, "connect %d fail2\n", this->port_);
    else if (mask == (ev_handler::all_events_mask | ev_handler::error_mask))
      ;//fprintf(stderr, "connect %d fail3\n", this->port_);

    //inet_address addr(this->port_, remote_addr);
    //i_connector.connect(new client(this->port_), addr, time_value(2, 0)); 

    delete this;
    return 0;
  }

private:
  int port_;
};

class loop_timer : public timer_handler
{
public:
  virtual int handle_timeout()
  {
    int ret = 0;
    for (int i = 30; i < 624; ++i) {
      inet_address addr(i, remote_addr);
      ret = i_connector.connect(new client(i), addr, time_value(2, 0)); 
      if (ret == -1) {
        //fprintf(stderr, "connect %d fail\n", i);
        continue;
      }
    }
    struct timeval now;
    ::gettimeofday(&now, NULL);

    char log_time[64] = {0};
    struct tm tm_;
    ::localtime_r(&(now.tv_sec), &tm_);
    ::strftime(log_time, sizeof(log_time), "%Y-%m-%d %H:%M:%S", &tm_);
    fprintf(stderr, "[%s.%03d]loop one\n", log_time, (int)((now.tv_usec + 999) / 1000));
    return 0;
  }
};

class signal_hup : public signal_handler
{
public:
  virtual int handle_signal()
  {
    fprintf(stderr, "catch signal hup\n");
    return 0;
  }
};

int main(int argc, char *argv[])
{
  remote_addr = argv[1];
  signal(SIGPIPE, SIG_IGN);
  signal(SIGHUP, SIG_IGN);

  reactor r;
  int ret = r.open();
  if (ret != 0) {
    fprintf(stderr, "open failed![%s]\n", strerror(errno));
    return -1;
  }

  ret = i_connector.open(&r);
  if (ret == -1) {
    fprintf(stderr, "acceptor open failed![%s]\n", strerror(errno));
    return -1;
  }
  r.schedule_timer(new loop_timer(), time_value(1, 0), time_value(2, 500*1000));
  r.register_signal(new signal_hup(), SIGHUP);
  r.run_event_loop();
  return 0;
}
