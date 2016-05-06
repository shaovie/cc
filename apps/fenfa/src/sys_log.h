// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw
 * Date     : 2016-05-06 15:22
 */
//========================================================================

#ifndef SYS_LOG_H_
#define SYS_LOG_H_

// Lib header
#include "ilog.h"
#include "singleton.h"

// Forward declarations

#define LOG_CFG_DIR    "cfg"

// @brief for output common log record
class sys_log : public singleton<sys_log>, public ilog
{
  friend class singleton<sys_log>;
private:
  sys_log() {};
};

// @brief for output error/fatal log record
class err_log : public singleton<err_log>, public ilog
{
  friend class singleton<err_log>;
private:
  err_log() {};
};

extern int init_sys_log(const char *);

#endif // SYS_LOG_H_
