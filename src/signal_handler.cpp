#include "signal_handler.h"

#include <unistd.h>
#include <sys/signalfd.h>

signal_handler::signal_handler() :
  svc_handler()
{ }
int signal_handler::handle_input(const int handle)
{
  struct signalfd_siginfo fdsi;
  ::read(handle, &fdsi, sizeof(struct signalfd_siginfo));

  return this->handle_signal();
}
