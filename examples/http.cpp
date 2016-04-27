#include "reactor.h"
#include "async_reactor_notify.h"
#include "svc_handler.h"
#include "timer_handler.h"
#include "signal_handler.h"
#include "acceptor.h"
#include "mblock.h"
#include "process.h"
#include "guard.h"
#include "task.h"
#include "thread_mutex.h"
#include "ilog.h"
#include "ilist.h"
#include "singleton.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <cassert>
#include <signal.h>
#include <cctype>
#include <sched.h>

//#define TP_REACTOR 1

int64_t g_total_request = 1;

class sys_log : public singleton<sys_log>, public ilog
{
  friend class singleton<sys_log>;
private:
  sys_log() {};
};

class err_log : public singleton<err_log>, public ilog
{
  friend class singleton<err_log>;
private:
  err_log() {};
};

static ilog_obj *e_log = err_log::instance()->get_ilog("base");
static ilog_obj *s_log = sys_log::instance()->get_ilog("base");

#define LOG_CFG_DIR    "cfg"
int init_sys_log(const char *svc_name)
{
  //= init log
  char bf[128] = {0};
  ::snprintf(bf, sizeof(bf), "%s/sys_log.%s", LOG_CFG_DIR, svc_name);
  if (sys_log::instance()->init(bf) != 0)
  {
    fprintf(stderr, "Error: ilog - init sys log failed!\n");
    return -1;
  }
  //
  ::snprintf(bf, sizeof(bf), "%s/err_log.%s", LOG_CFG_DIR, svc_name);
  if (err_log::instance()->init(bf) != 0)
  {
    fprintf(stderr, "Error: ilog - init err log failed!\n");
    return -1;
  }
  return 0;
}

class util
{
public:
  inline static char *get_http_value(const char *header,
                                     const char *key,
                                     size_t key_len,
                                     char **v_end)
  {
    char *key_p = ::strcasestr(const_cast<char *>(header), key);
    if (key_p == NULL) return NULL;
    key_p = ::strchr(key_p + key_len, ':');
    if (key_p == NULL || *(key_p + 1) == '\r')
      return NULL;

    ++key_p;
    key_p += ::strspn(key_p, " \t");

    size_t len = ::strcspn(key_p, "\r\n");
    if (len == 0) return NULL;

    if (v_end != NULL) {
      *v_end = key_p + len;

      while (*v_end > key_p && ::isspace(**v_end))
        --(*v_end);
      ++(*v_end);
    }
    return key_p;
  }
};

class worker : public task
{
public:
  worker(reactor *r, int cpu_idx) :
    cpu_idx_(cpu_idx),
    reactor_(r)
  { }
protected:
  virtual int svc()
  {
    if (process::pthread_setaffinity(pthread_self(), this->cpu_idx_) == -1)
      fprintf(stderr, "set thread affinity failed\n");
    this->reactor_->run_event_loop();
    return 0;
  }
private:
  int cpu_idx_;
  reactor *reactor_;
};
class client;
class client_dispatch : public async_reactor_notify
{
public:
  client_dispatch()
  { }
  void init(reactor *r)
  {
    this->set_reactor(r);
    this->open(r);
  }
  void notify_new_client(client *c)
  {
    {
      guard<thread_mutex> g(this->mtx_);
      this->new_client_list_.push_back(c);
    }
    this->notify();
  }
  virtual void handle_notify();
private:
  thread_mutex mtx_;
  ilist<client *> new_client_list_;
};
//
client_dispatch m_client_dispatch[4];

class client : public svc_handler
{
public:
  client() :
    recv_buff_(new mblock(2048))
  {
  }
  ~client()
  {
    this->recv_buff_->release();
  }
  virtual int open(ev_handler *eh)
  {
#ifdef TP_REACTOR
    m_client_dispatch[this->get_handle() % 3 + 1].notify_new_client(this);
#else
    this->set_reactor(eh->get_reactor()); // !!
    socket::set_nodelay(this->get_handle());
    int ret = this->get_reactor()->register_handler(this,
                                                    ev_handler::read_mask);
    assert(ret != -1);
#endif
    return 0;
  }
  int connect_ok(reactor *r)
  {
    this->set_reactor(r);
    socket::set_nodelay(this->get_handle());
    int ret = this->get_reactor()->register_handler(this,
                                                    ev_handler::read_mask);
    assert(ret != -1);
    return 0;
  }
  virtual int handle_input(const int handle)
  {
    int ret = socket::recv(handle,
                           this->recv_buff_->wr_ptr(), 
                           this->recv_buff_->space() - 1);
    if (ret == -1) {
      if (errno == EWOULDBLOCK)
        return 0;  // Maybe
      return -1;
    } else if (ret == 0) { // peer closed
      return -1;
    }
    this->recv_buff_->wr_ptr(ret);

    if (this->recv_buff_->space() == 1) {
      int len = this->recv_buff_->length();
      if (len != 0)
        ::memmove(this->recv_buff_->data(), this->recv_buff_->rd_ptr(), len);
      this->recv_buff_->reset();
      this->recv_buff_->wr_ptr(len);
    }

    this->recv_buff_->set_eof();

    int handle_one = false;
    do {
      ret = this->handle_data(handle);
      if (ret == 1) {
        handle_one = 1;
        if (this->recv_buff_->length() == 0)
          break;
      }
    } while (ret == 1);

    if (handle_one == 1)
      ;//this->recv_buff_->rd_ptr(1);

    return ret == -1 ? -1 : 0;
  }
  int handle_data(const int handle)
  {
    static int count = 1;
    char *req = this->recv_buff_->rd_ptr();
    char *header_end = ::strstr(req, "\r\n\r\n");
    if (header_end == NULL) {
      //e_log->rinfo("uncomplete http header[%s] space = %d", req, this->recv_buff_->space());
      return 0;
    }
    header_end += 4;
    int http_head_len = header_end - req;
    char *content_length_p = util::get_http_value(req, 
                                                  "Content-Length",
                                                  sizeof("Content-Length") - 1,
                                                  NULL);
    int content_length = 0;
    if (content_length_p != NULL) {
      content_length = ::atoi(content_length_p);
      if (this->recv_buff_->length() - http_head_len < content_length)
        return 0;
    }
    this->recv_buff_->rd_ptr(http_head_len + content_length);

    if ((req[0] == 'G' || req[0] == 'g')
        && (req[1] == 'E' || req[1] == 'e')
        && (req[2] == 'T' || req[2] == 't')
       ) {
      char *connection_p = util::get_http_value(req, 
                                                "Connection",
                                                sizeof("Connection") - 1,
                                                NULL);
      static char http_resp[] = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 12\r\n\r\nhello world!";
      static char http_resp_k[] = "HTTP/1.1 200 OK\r\nConnection: keep-alive\r\nContent-Length: 12\r\n\r\nhello world!";
      if (connection_p == NULL || connection_p[0] == 'c' || connection_p[0] == 'C') {
        g_total_request++;
        socket::send(handle, http_resp, sizeof(http_resp) - 1);
        return -1;
      } else {
        g_total_request++;
        int ret = socket::send(handle, http_resp_k, sizeof(http_resp_k) - 1);
        if (ret == -1)
          return -1;
        return 1;
      }
    }
    e_log->rinfo("unsupport ! %s", req);
    return -1;
  }
  virtual int handle_close(reactor_mask )
  {
    delete this;
    return 0;
  }

  mblock *recv_buff_;
};
void client_dispatch::handle_notify()
{
  while (true) {
    client *c = NULL;
    {
      guard<thread_mutex> g(this->mtx_);
      if (this->new_client_list_.empty())
        return ;
      c = this->new_client_list_.pop_front();
    }
    if (c->connect_ok(this->get_reactor()) != 0)
      c->close(ev_handler::accept_mask);
  }
}
class loop_timer : public timer_handler
{
public:
  virtual int handle_timeout()
  {
    struct timeval now;
    ::gettimeofday(&now, NULL);

    char log_time[64] = {0};
    struct tm tm_;
    ::localtime_r(&(now.tv_sec), &tm_);
    ::strftime(log_time, sizeof(log_time), "%Y-%m-%d %H:%M:%S", &tm_);
    fprintf(stderr, "[%s.%03d] processed %ld \n", log_time, (int)((now.tv_usec + 999) / 1000), g_total_request);
    return 0;
  }
};
class signal_int : public signal_handler
{
public:
  virtual int handle_signal()
  {
    ::exit(1);
    return 0;
  }
};
int main()
{
  int port = 7777;

  signal(SIGPIPE, SIG_IGN);
  signal(SIGHUP, SIG_IGN);

  init_sys_log("t");

  cpu_set_t mask;
  CPU_ZERO(&mask);
#ifdef TP_REACTOR
  CPU_SET(0, &mask);
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0)
    fprintf(stderr, "set main thread affinity failed\n");
  reactor m_reactor[4];
  for (size_t i = 0; i < sizeof(m_reactor) / sizeof(m_reactor[0]); ++i)
  {
    if (m_reactor[i].open() != 0) {
      fprintf(stderr, "open failed![%s]\n", strerror(errno));
      return -1;
    }
    m_client_dispatch[i].init(&(m_reactor[i]));
  }
  inet_address local_addr(port, "0.0.0.0");
  acceptor<client> i_acceptor;
  int ret = i_acceptor.open(local_addr, &(m_reactor[0]));
  if (ret == -1) {
    fprintf(stderr, "acceptor open failed![%s]\n", strerror(errno));
    return -1;
  }

  for (size_t i = 1; i < sizeof(m_reactor) / sizeof(m_reactor[0]); ++i) {
    worker *w = new worker(&(m_reactor[i]), i);
    if (w->activate(1, 1024) != 0)
      return -1;
  }
  m_reactor[0].run_event_loop();
#else
  CPU_SET(2, &mask);
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0)
    fprintf(stderr, "set main thread affinity failed\n");
  reactor r;
  int ret = r.open();
  if (ret != 0) {
    fprintf(stderr, "open failed![%s]\n", strerror(errno));
    return -1;
  }

  inet_address local_addr(port, "0.0.0.0");
  acceptor<client> i_acceptor;
  ret = i_acceptor.open(local_addr, &r);
  if (ret == -1) {
    fprintf(stderr, "acceptor open failed![%s]\n", strerror(errno));
    return -1;
  }
  r.schedule_timer(new loop_timer(), time_value(1, 0), time_value(10, 0));
  r.register_signal(new signal_int(), SIGINT);
  r.run_event_loop();
#endif
  return 0;
}
