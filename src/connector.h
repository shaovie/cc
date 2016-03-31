// -*- C++ -*-

//========================================================================
/**
 * Author   : cuisw
 * Date     : 2012-04-28 01:24
 */
//========================================================================

#ifndef CONNECTOR_H_
#define CONNECTOR_H_

#include "ilist.h"
#include "socket.h"
#include "inet_address.h"
#include "ev_handler.h"
#include "timer_handler.h"

template<typename svc_handler_t> class connector;
template<typename svc_handler_t> class nonblocking_connect_handler;

template<typename svc_handler_t>
class nonblocking_connect_timer : public timer_handler
{
  friend class nonblocking_connect_handler<svc_handler_t>;
public:
  nonblocking_connect_timer(nonblocking_connect_handler<svc_handler_t> *nch) :
    timer_handler(),
    nch_(nch)
  { }

  // called if a connection times out before completing.
  virtual int handle_timeout()
  {
    this->nch_->close();

    svc_handler_t *sh = this->nch_->get_svc_handler();
    sh->connect_timeout();

    return 0;
  }
  virtual int close(reactor_mask)
  {
    this->shutdown_i();
    return 0;
  }
private:
  nonblocking_connect_handler<svc_handler_t> *nch_;
};

template<typename svc_handler_t>
class nonblocking_connect_handler : public ev_handler
{
  friend class connector<svc_handler_t>;
public:
  nonblocking_connect_handler(connector<svc_handler_t> &cn) :
    connector_(cn),
    svc_handler_(NULL),
    timer_(new nonblocking_connect_timer<svc_handler_t>(this))
  { }

  void close()
  {
    // clean events
    this->get_reactor()->remove_handler(this,
                                        ev_handler::all_events_mask
                                        | ev_handler::dont_call);

    this->timer_->close(ev_handler::connect_mask);
  }

  int connect(const time_value &timeout)
  {
    if (this->get_reactor()->register_handler(this, ev_handler::connect_mask) == -1
        || this->get_reactor()->schedule_timer(this->timer_, timeout) == -1) {
      this->handle_close(ev_handler::connect_mask);
      return -1;
    }

    return 0;
  }

  inline int get_handle() const
  { return this->svc_handler_->get_handle(); }
  inline void set_handle(const int handle)
  { this->svc_handler_->set_handle(handle); }

  inline svc_handler_t *get_svc_handler()
  { return this->svc_handler_; }
  inline void set_svc_handler(svc_handler_t *sh)
  { this->svc_handler_ = sh; }

  // called by reactor when asynchronous connections succeed. 
  virtual int handle_output(const int handle)
  {
    int err = 0;
    socket::getsock_error(handle, err);
    if (err != 0)
      return -1;

    this->close();
    this->connector_.init_svc_handler(this->svc_handler_, handle);

    this->connector_.release_con_handler(this);
    return 0;
  }

  virtual int handle_close(reactor_mask mask)
  {
    this->close();
    this->svc_handler_->close(mask);

    this->connector_.release_con_handler(this);
    return 0;
  }
private:
  //
  connector<svc_handler_t> &connector_;
  //
  svc_handler_t *svc_handler_;

  nonblocking_connect_timer<svc_handler_t> *timer_;
};

template<typename svc_handler_t>
class connector : public ev_handler
{
  friend class nonblocking_connect_handler<svc_handler_t>;
public:
  connector() { }

  inline int open(reactor *r)
  {
    this->set_reactor(r);
    return 0;
  }

  int connect(svc_handler_t *sh, 
              const inet_address &remote_addr, 
              const time_value &timeout,
              const size_t rcvbuf_size = 0)
  {
    if (sh == NULL)
      return -1;

    sh->set_reactor(this->get_reactor());

    if (this->connect_i(sh, remote_addr, rcvbuf_size) == 0) {
      if (sh->open(this) == 0)
        return 0;
    } else if (errno == EINPROGRESS) {
      if (timeout != time_value::zero)
        return this->nonblocking_connect(sh, timeout);
    }
    sh->close(ev_handler::connect_mask);
    return -1;
  }
protected:
  int connect_i(svc_handler_t *sh,
                const inet_address &remote_addr,
                const size_t recvbuf_size)
  {
    int handle = sh->get_handle();
    if (handle == -1) {
      handle = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
      if (handle == -1)
        return -1;
    }

    if (recvbuf_size != 0 
        && socket::set_rcvbuf(handle, recvbuf_size) == -1) {
      ::close(handle);
      return -1;
    }
    //
    sh->set_handle(handle);

    return ::connect(handle, 
                     reinterpret_cast<sockaddr *>(remote_addr.get_addr()),
                     remote_addr.get_addr_size());
  }
  int nonblocking_connect(svc_handler_t *sh, const time_value &timeout)
  {
    if (this->free_con_handler_list_.empty())
      this->free_con_handler_list_.push_back(new nonblocking_connect_handler<svc_handler_t>(*this));
    nonblocking_connect_handler<svc_handler_t> *nbch = this->free_con_handler_list_.pop_front();
    nbch->set_reactor(this->get_reactor());
    nbch->set_svc_handler(sh);

    return nbch->connect(timeout);
  }
  void init_svc_handler(svc_handler_t *sh, const int handle)
  {
    sh->set_handle(handle);
    if (sh->open(this) != 0)
      sh->close(ev_handler::connect_mask);
  }
  void release_con_handler(nonblocking_connect_handler<svc_handler_t> *nbch)
  {
    this->free_con_handler_list_.push_back(nbch);
  }

private:
  ilist<nonblocking_connect_handler<svc_handler_t> *> free_con_handler_list_;
};
#endif // CONNECTOR_H_

