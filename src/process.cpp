#include "process.h"
#include "date_time.h"

#include <pwd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/file.h>

extern char **environ;

static int s_child_pid = -1;

char **process::save_argv(int argc, char *argv[])
{
  char **new_argv = (char **)::malloc((argc + 1) * sizeof(char *));
  new_argv[argc] = NULL;
  for (int i = 0; i < argc; ++i)
    new_argv[i] = ::strdup(argv[i]);

  return new_argv;
}
void process::set_proc_title(char *argv[], const char *title)
{
  static int argv_env_total_len = 0;
  if (argv_env_total_len > 0) {
    ::strncpy(argv[0], title, argv_env_total_len);
    return ;
  }
  size_t env_size = 0;
  for (int i = 0; environ[i]; ++i)
    env_size += ::strlen(environ[i]) + 1;
  
  char *env_p = (char *)::malloc(env_size);
  char *title_end_p = argv[0];
  for (int i = 0; argv[i]; ++i) {
    if (title_end_p == argv[i])
      title_end_p = argv[i] + ::strlen(argv[i]) + 1;
  }

  for (int i = 0; environ[i]; ++i) {
    if (title_end_p == environ[i]) {
      env_size = ::strlen(environ[i]) + 1;
      title_end_p = environ[i] + env_size;

      ::strncpy(env_p, environ[i], env_size);
      environ[i] = env_p;
      env_p += env_size;
    }
  }

  title_end_p--; // reserve '\0'

  argv[1] = NULL;
  argv_env_total_len = title_end_p - argv[0];
  ::strncpy(argv[0], title, argv_env_total_len);
}
int process::set_max_fds(const int maxfds)
{
  struct rlimit limit;
  limit.rlim_cur = maxfds;
  limit.rlim_max = maxfds;

  return ::setrlimit(RLIMIT_NOFILE, &limit);
}
int process::get_max_fds()
{
  struct rlimit rl;
  ::memset((void *)&rl, 0, sizeof(rl));
  int r = ::getrlimit(RLIMIT_NOFILE, &rl);
  if (r == 0 && rl.rlim_cur != RLIM_INFINITY)
    return rl.rlim_cur;
  return -1;
}
int process::runas(const char *uid)
{
  struct passwd *pw = NULL;
  if ((pw = ::getpwnam(uid)) == NULL)
    return -1;
  else if (::setgid(pw->pw_gid) == -1)
    return -1;
  else if (::setuid(pw->pw_uid) == -1)
    return -1;
  ::endpwent();
  return 0;
}
int process::dump_corefile()
{
  if (::prctl(PR_SET_DUMPABLE, 1) == -1) {
    fprintf(stderr, "Error: prctl - failed to prctl!\n");
    return -1;
  }

  struct rlimit rlim_new;
  struct rlimit rlim;
  if (::getrlimit(RLIMIT_CORE, &rlim) == 0) {
    rlim_new.rlim_cur = rlim_new.rlim_max = RLIM_INFINITY;
    if (::setrlimit(RLIMIT_CORE, &rlim_new) != 0) 
      return -1;
  }

  if ((::getrlimit(RLIMIT_CORE, &rlim) != 0) || rlim.rlim_cur == 0) {
    fprintf(stderr, "Error: failed to ensure corefile creation\n");
    return -1;
  }
  return 0;
}
int process::daemon()
{
  int child_pid = ::fork();
  if (child_pid < 0) {
    fprintf(stderr, "Error: fork failed! %s\n", ::strerror(errno));
    ::exit(1);
  } else if (child_pid > 0)
    ::exit(0);

  if (::setsid() == -1) {
    fprintf(stderr, "Error: setsid failed! %s\n", ::strerror(errno));
    return -1;
  }

  ::umask(0);

  int fd = ::open("/dev/null", O_RDWR, 0);
  if (fd != -1) {
    ::dup2(fd, STDIN_FILENO);
    ::dup2(fd, STDOUT_FILENO);
    ::dup2(fd, STDERR_FILENO);

    if (fd > STDERR_FILENO)
      ::close(fd);
  }
  return 0;
}
int process::output_pid(const char *path, const bool exclusive/* = true*/)
{
  if (path == NULL) {
    fprintf(stderr, "Error: output_pid failed! path is null");
    return -1;
  }
  int fd = ::open(path, O_CREAT | O_WRONLY, 0644);
  if (fd == -1) {
    fprintf(stderr, "Error: open %s failed! %s\n", path, ::strerror(errno));
    return -1;
  }
  if (exclusive && ::flock(fd, LOCK_EX | LOCK_NB) != 0) {
    fprintf(stderr, "Error: anather process is running or flock error!\n");
    ::exit(1);
  }
  ::ftruncate(fd, 0);
  char bf[32] = {0};
  int len = ::snprintf(bf, sizeof(bf), "%d", ::getpid());
  return ::write(fd, bf, len) > 0 ? 0 : -1;
}
void process::to_guard_mode(const char *log_path)
{
  if (s_child_pid == 0)
    return ;

  while (true) {
    s_child_pid = ::fork();
    if (s_child_pid < 0) {
      fprintf(stderr, "Error: fork failed! %s\n", ::strerror(errno));
      ::exit(1);
    } else if (s_child_pid == 0)
      break;
    
    // parent process !
    int status = 0;
    if (::waitpid(s_child_pid, &status, 0) < 0) {
      fprintf(stderr, "Error: fork failed! %s\n", ::strerror(errno));
      ::exit(1);
    }

    int fd = ::open(log_path, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd == -1) {
      fprintf(stderr, "Error: open %s failed! %s\n", log_path, ::strerror(errno));
      ::exit(1);
    }
    char err[128] = {0};
    char time_bf[32] = {0};
    date_time().to_str(time_bf, sizeof(time_bf));
    if (WIFEXITED(status)) { //is non-zero if the child exited normally
      int len = ::snprintf(err,
                           sizeof(err),
                           "%s child proceess(%d) exit! code = %d\n",
                           time_bf, s_child_pid, WEXITSTATUS(status));
      ::write(fd, err, len);
      ::close(fd);
      ::exit(0);
    }
    if (WIFSIGNALED(status)) { //program exit by a siganl but not catched
      int len = ::snprintf(err,
                           sizeof(err),
                           "%s child proceess(%d) exit by a signal(%d)\n",
                           time_bf, s_child_pid, WTERMSIG(status));
      ::write(fd, err, len);
      ::close(fd);
    }

    ::sleep(1);
  }
}
int process::child_pid()
{
  return s_child_pid;
}
int process::pthread_setaffinity(pthread_t thr_id, const int cpu_id)
{
  cpu_set_t cm;
  CPU_ZERO(&cm);
  CPU_SET(cpu_id, &cm);

  return ::pthread_setaffinity_np(thr_id, sizeof(cm), &cm) != 0 ? -1 : 0;
}

