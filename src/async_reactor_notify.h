// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw
 * Date     : 2012-04-26 16:56
 */
//========================================================================

#ifndef ASYNC_REACTOR_NOTIFY_H_
#define ASYNC_REACTOR_NOTIFY_H_

#include "ev_handler.h"

class async_reactor_notify : public ev_handler
{
public:
  async_reactor_notify();
  ~async_reactor_notify();

  int open(reactor *r);
  void close();
  void notify();

  virtual int get_handle() const { return this->eventfd_; }

  virtual int handle_input(const int );
  virtual void handle_notify() = 0;
private:
  int eventfd_;
};
#endif // ASYNC_REACTOR_NOTIFY_H_

