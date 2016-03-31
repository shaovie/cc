// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw
 * Date     : 2016-03-31 10:08
 */
//========================================================================

#ifndef TIMER_HANDLER_H_
#define TIMER_HANDLER_H_

#include "svc_handler.h"

class timer_handler : public svc_handler
{
public:
  timer_handler() :
    svc_handler()
  { }

  virtual int handle_input(const int handle)
  {
    uint64_t exp = 0;
    ::read(handle, &exp, sizeof(uint64_t));

    return this->handle_timeout();
  }

  virtual int handle_timeout() = 0;
};

#endif // TIMER_HANDLER_H_

