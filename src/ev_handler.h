// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw
 * Date     : 2012-04-28 01:24
 */
//========================================================================

#ifndef EV_HANDLER_H_
#define EV_HANDLER_H_

#include <cstddef>

// Forward declarations
class reactor; 

typedef int reactor_mask;

class ev_handler
{
public:
  enum 
  {
    null_mask       = 0,
    error_mask      = 1L << 1,
    read_mask       = 1L << 2,
    write_mask      = 1L << 3,
    accept_mask     = 1L << 4,
    connect_mask    = 1L << 5,
    epollet_mask    = 1L << 6,
    dont_call       = 1L << 31,

    all_events_mask = read_mask  | \
                      write_mask   |
                      accept_mask  |
                      epollet_mask |
                      connect_mask ,
  };
public:
  virtual ~ev_handler() {}

  // the handle_close(int, reactor_mask) will be called if below functions return -1. 
  virtual int handle_input(const int /*handle*/)  { return -1; }
  virtual int handle_output(const int /*handle*/) { return -1; }
  virtual int connect_timeout() { return -1; }

  virtual int handle_close(reactor_mask /*mask*/)
  { return -1; }

  virtual int get_handle() const { return -1; }
  virtual void set_handle(const int /*handle*/) { }

  void set_reactor(reactor *r) { this->reactor_ = r; }
  reactor *get_reactor(void) const { return this->reactor_; }
protected:
  ev_handler() :
    reactor_(NULL)
  { }

  reactor *reactor_;
};

#endif // EV_HANDLER_H_

