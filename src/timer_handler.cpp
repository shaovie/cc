#include "timer_handler.h"

#include <unistd.h>

timer_handler::timer_handler() :
  svc_handler()
{ }
int timer_handler::handle_input(const int handle)
{
  uint64_t exp = 0;
  ::read(handle, &exp, sizeof(uint64_t));

  return this->handle_timeout();
}
