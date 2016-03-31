// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw
 * Date     : 2012-04-28 10:08
 */
//========================================================================

#ifndef SVC_HANDLER_H_
#define SVC_HANDLER_H_

#include "socket.h"
#include "reactor.h"
#include "ev_handler.h"

class svc_handler : public ev_handler
{
public:
  svc_handler() : handle_(-1) { }

  virtual ~svc_handler()
  { this->shutdown_i(); }

  virtual int open(ev_handler *) { return -1; }

  virtual int close(reactor_mask mask)
  { return this->handle_close(mask); }

  virtual int handle_close(reactor_mask )
  {
    delete this;
    return 0;
  }

  virtual void set_remote_addr(const inet_address &) { }

  virtual int get_handle() const { return this->handle_; }
  virtual void set_handle(const int handle) { this->handle_ = handle; }

protected:
  void shutdown_i(void)
  {
    if (this->get_reactor()) {
      reactor_mask mask = ev_handler::all_events_mask|ev_handler::dont_call;

      if (this->get_handle() != -1)
        this->get_reactor()->remove_handler(this, mask);
    }
    //
    if (this->get_handle() != -1) {
      ::close(this->get_handle());
      this->set_handle(-1);
    }
    this->set_reactor(NULL);
  }
protected:
  int handle_;
};

#endif // SVC_HANDLER_H_

