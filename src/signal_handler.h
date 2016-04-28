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

class signal_handler : public svc_handler
{
public:
  signal_handler();

  virtual int handle_input(const int handle);

  virtual int handle_signal() = 0;
};

#endif // SIGNAL_HANDLER_H_

