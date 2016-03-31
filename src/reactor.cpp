#include "reactor.h"
#include "macros.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/timerfd.h>

namespace reactor_help
{
  inline int reactor_mask_to_epoll_event(reactor_mask mask)
  {
    int events = ev_handler::null_mask;
    if (BIT_ENABLED(mask, ev_handler::read_mask | ev_handler::accept_mask))
      SET_BITS(events, EPOLLIN);

    if (BIT_ENABLED(mask, ev_handler::connect_mask))
      SET_BITS(events, EPOLLOUT);

    if (BIT_ENABLED(mask, ev_handler::write_mask))
      SET_BITS(events, EPOLLOUT);

    return events;
  }
}
class reactor_event_tuple
{
public:
  reactor_event_tuple() :
    eh_(NULL),
    mask_(ev_handler::null_mask)
  { }

  ev_handler *eh_;
  reactor_mask  mask_;
};
//=
reactor::reactor() :
  max_fds_(0),
  epoll_fd_(-1),
  events_(NULL),
  handler_rep_(NULL)
{ }
int reactor::open()
{
  struct rlimit rl;
  ::memset((void *)&rl, 0, sizeof(rl));
  int r = ::getrlimit(RLIMIT_NOFILE, &rl);
  if (r == 0 && rl.rlim_cur != RLIM_INFINITY)
    this->max_fds_ = rl.rlim_cur - 4;

  // /proc/sys/fs/epoll/max_user_watches (since Linux 2.6.28)
  // Each registered file descriptor costs roughly 160 bytes on a 64-bit kernel.
  this->epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);

  if (this->max_fds_ <= 0
      || this->epoll_fd_ == -1)
    return -1;

  this->events_      = new epoll_event[this->max_fds_]();
  this->handler_rep_ = new reactor_event_tuple[this->max_fds_]();

  for (int i = 0; i < this->max_fds_; ++i) {
    this->handler_rep_[i].eh_   = NULL;
    this->handler_rep_[i].mask_ = ev_handler::null_mask;
  }

  return 0;
}
int reactor::register_handler(ev_handler *eh, reactor_mask mask)
{
  if (mask == ev_handler::null_mask)
    return -1;

  int handle = eh->get_handle();
  if (handle < 0 || handle >= this->max_fds_)
    return -1;

  struct epoll_event epev;
  ::memset(&epev, 0, sizeof(epoll_event));
  epev.data.ptr = eh;

  reactor_event_tuple &r_et = this->handler_rep_[handle];
  if (r_et.eh_ == NULL) {
    epev.events = reactor_help::reactor_mask_to_epoll_event(mask);
    if (::epoll_ctl(this->epoll_fd_, EPOLL_CTL_ADD, handle, &epev) == 0
        || (errno == EEXIST
            && ::epoll_ctl(this->epoll_fd_, EPOLL_CTL_MOD, handle, &epev) == 0)) {
      r_et.eh_   = eh;
      r_et.mask_ = mask;
      return 0;
    }
  } else {
    epev.events = reactor_help::reactor_mask_to_epoll_event(r_et.mask_ | mask);
    if (::epoll_ctl(this->epoll_fd_, EPOLL_CTL_MOD, handle, &epev) == 0
        || (errno == ENOENT
            // If a fd is closed, epoll removes it from the poll set
            // automatically - we may not know about it yet. If that's the
            // case, a mod operation will fail with ENOENT. Retry it as
            // an add.
            && ::epoll_ctl(this->epoll_fd_, EPOLL_CTL_ADD, handle, &epev) == 0)) {
      SET_BITS(r_et.mask_, mask);
      return 0;
    }
  }

  return -1;
}
int reactor::remove_handler(ev_handler *eh, reactor_mask mask)
{
  int handle = eh->get_handle();
  if (handle < 0 || handle >= this->max_fds_)
    return -1;

  if (BIT_DISABLED(mask, ev_handler::dont_call))
    eh->handle_close(mask);

  reactor_event_tuple &r_et = this->handler_rep_[handle];
  if (r_et.eh_ == NULL)
    return 0; // already removed

  if (r_et.mask_ & (~mask)) {
    struct epoll_event epev;
    ::memset(&epev, 0, sizeof(epoll_event));
    epev.data.ptr = eh;

    epev.events = reactor_help::reactor_mask_to_epoll_event(r_et.mask_ & (~mask));
    if (::epoll_ctl(this->epoll_fd_, EPOLL_CTL_MOD, handle, &epev) == 0
        || (errno == ENOENT
            // If a handle is closed, epoll removes it from the poll set
            // automatically - we may not know about it yet. If that's the
            // case, a mod operation will fail with ENOENT. Retry it as
            // an add.
            && ::epoll_ctl(this->epoll_fd_, EPOLL_CTL_ADD, handle, &epev) == 0)) {
      CLR_BITS(r_et.mask_, mask);
      return 0;
    }
  } else {
    ::epoll_ctl(this->epoll_fd_, EPOLL_CTL_DEL, handle, NULL);
    r_et.eh_   = NULL;
    r_et.mask_ = ev_handler::null_mask;
    return 0;
  }

  return -1;
}
int reactor::run_event_loop()
{
  int nfds = 0;
  while (true) {
    do {
      nfds = ::epoll_wait(this->epoll_fd_, this->events_, this->max_fds_, -1);
    } while (nfds == -1 && errno == EINTR);

    if (nfds > 0) {
      this->dispatch_events(nfds);
    } else {
      break;
    }
  }
  return -1;
}
void reactor::dispatch_events(const int nfds)
{
  for (int i = 0; i < nfds; ++i) {
    struct epoll_event &one_event = this->events_[i];
    ev_handler *eh = reinterpret_cast<ev_handler *>(one_event.data.ptr);
    if (eh == NULL)
      continue;
    
    if (BIT_ENABLED(one_event.events, EPOLLHUP | EPOLLERR)) {
      // Note that if there's an error(such as the handle was closed
      // without being removed from the event set) the EPOLLHUP and/or
      // EPOLLERR bits will be set in events.
      eh->handle_close(ev_handler::all_events_mask | ev_handler::error_mask);
      continue;
    }

    int handle = eh->get_handle();
    if (BIT_ENABLED(one_event.events, EPOLLIN)) {
      if (eh->handle_input(handle) < 0)
        eh->handle_close(ev_handler::read_mask);
    }
    if (BIT_ENABLED(one_event.events, EPOLLOUT)) {
      if (this->handler_rep_[handle].eh_ == NULL)
        continue;
      if (eh->handle_output(handle) < 0)
        eh->handle_close(ev_handler::write_mask);
    }
  }
}
int reactor::schedule_timer(ev_handler *eh,
                            const time_value &delay,
                            const time_value &interval/* = time_value::zero*/)
{
  int fd = ::timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
  if (fd == -1)
    return -1;

  struct itimerspec new_value;
  new_value.it_value.tv_sec  = delay.sec();
  new_value.it_value.tv_nsec = delay.usec() * 1000;
  new_value.it_interval.tv_sec  = interval.sec();
  new_value.it_interval.tv_nsec = interval.usec() * 1000;
  if (::timerfd_settime(fd, 0, &new_value, NULL) == -1)
    return -1;

  eh->set_handle(fd);
  eh->set_reactor(this);
  return this->register_handler(eh, ev_handler::read_mask);
}
