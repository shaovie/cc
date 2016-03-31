// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw
 * Date     : 2012-04-26 16:56
 */
//========================================================================

#ifndef REACTOR_H_
#define REACTOR_H_

#include "ev_handler.h"
#include "time_value.h"

// Forward declarations
struct epoll_event;
class reactor_event_tuple;

class reactor
{
public:
  reactor();

  int open();

  int run_event_loop();

  int register_handler(ev_handler *eh, reactor_mask mask);

  int remove_handler(ev_handler *eh, reactor_mask mask);

  int schedule_timer(ev_handler *eh,
                     const time_value &delay,
                     const time_value &interval = time_value::zero);

  int register_signal(ev_handler *eh, const int signum);
private:
  void dispatch_events(const int nfds);
private:
  reactor(const reactor &);
  reactor &operator = (const reactor &);
private:
  int max_fds_;
  int epoll_fd_;

  struct epoll_event *events_;

  reactor_event_tuple *handler_rep_;
};
#endif // REACTOR_H_

