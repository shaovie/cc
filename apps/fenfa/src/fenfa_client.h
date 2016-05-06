// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw
 * Date     : 2016-05-06 15:12
 */
//========================================================================

#ifndef SYS_H_
#define SYS_H_

// Forward declarations
class reactor;

class fenfa_client
{
public:
  static int init_svc();

  static void to_friendly_exit();
  static void emergency_exit(const int sig);

public:
  static bool friendly_exit;

  static int svc_launch_time;

  static reactor *r;
};
#endif // SYS_H_
