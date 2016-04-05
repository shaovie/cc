// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw
 * Date     : 2016-04-05 10:26
 */
//========================================================================

#ifndef THREAD_H_
#define THREAD_H_

#include <cstddef>

// Forward declarations

class thread
{
public:
  thread() { }
  virtual ~thread() { }

  int create();
protected:
  virtual int svc() = 0;

private:
  thread(const thread &);
  thread &operator=(const thread &);

  static void *svc_run(void *arg);
};

#endif // THREAD_H_

