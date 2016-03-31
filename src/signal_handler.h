// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw
 * Date     : 2016-03-31 10:08
 */
//========================================================================

#ifndef SIGNAL_HANDLER_H_
#define SIGNAL_HANDLER_H_

#include "svc_handler.h"

#include <sys/signalfd.h>

class signal_handler : public svc_handler
{
public:
  signal_handler() :
    svc_handler()
  { }

  virtual int handle_input(const int handle)
  {
    struct signalfd_siginfo fdsi;
    ::read(handle, &fdsi, sizeof(struct signalfd_siginfo));

    return this->handle_signal();
  }

  virtual int handle_signal() = 0;
};

#endif // SIGNAL_HANDLER_H_

