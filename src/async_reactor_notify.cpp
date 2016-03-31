#include "async_reactor_notify.h"
#include "reactor.h"

#include <unistd.h>
#include <sys/eventfd.h>

async_reactor_notify::async_reactor_notify() :
  eventfd_(-1)
{
}
async_reactor_notify::~async_reactor_notify()
{
  this->close();
}
int async_reactor_notify::open(reactor *r)
{
  this->eventfd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (this->eventfd_ == -1)
    return -1;

  this->set_reactor(r);
  this->set_handle(this->eventfd_);

  if (this->get_reactor()->register_handler(this, ev_handler::read_mask) != 0) {
    this->close();
    return -1;
  }
  return 0;
}
int async_reactor_notify::handle_input(const int handle)
{
  uint64_t data = 0;
  ::read(handle, &data, sizeof(data));

  this->handle_notify();
  return 0;
}
void async_reactor_notify::notify()
{
  uint64_t data = 1;
  ::write(this->eventfd_, &data, sizeof(data));
}
void async_reactor_notify::close()
{
  if (this->get_reactor() != NULL)
    this->get_reactor()->remove_handler(this, ev_handler::read_mask);
  this->set_reactor(NULL);
  if (this->eventfd_ != -1)
    ::close(this->eventfd_);
  this->eventfd_ = -1;
}

