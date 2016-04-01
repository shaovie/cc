// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw
 * Date     : 2012-04-26 19:59
 */
//========================================================================

#ifndef PROCESS_H_
#define PROCESS_H_

class process
{
public:
  static int set_max_fds(const int maxfds);
  static int get_max_fds();

  static int runas(const char *uid);

  static int dump_corefile();

  static int daemon();

  static int output_pid(const char *path, const bool exclusive = true);

  // to change the process title
  static void set_proc_title(char *argv[], const char *title);
  // return new argv memory pointer
  static char **save_argv(int argc, char *argv[]);

  static void to_guard_mode(const char *log_path);
  static int child_pid();
};

#endif // PROCESS_H_

