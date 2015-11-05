
/*
 * Copyright (C) jlijian3@gmail.com
 */

#ifndef __PS_THREAD_INCLUDE__
#define __PS_THREAD_INCLUDE__

#include <event.h>
#include <pthread.h>
#include <errno.h>

#include "queue.h"
#include "connection.h"
#include "base.h"

struct cq_item {
  cq_item() {}

  cq_item(int fd, enum conn_states state, int evflags) :
    sfd(fd),
    init_state(state),
    event_flags(evflags)
  {
  }
  int               sfd;
  enum conn_states  init_state;
  int               event_flags;
  struct sockaddr_storage addr;
};

class LibeventThread : public BaseThread {
public: 
  LibeventThread() : _base(NULL) {
  }

  ~LibeventThread() {
    if (_base)
      event_base_free(_base);
  }
  
  bool init();
  bool stop();

  struct event_base *get_event_base() {
    return _base; 
  }

  void set_event_base(struct event_base *base) {
    _base = base; 
  }
  
  void cq_notify() {
    /* 
    if (write(_notify_send_fd, "", 1) != 1) {
      perror("Writing to thread notify pipe"); 
    }
    */

    int rv, cnt = 0;
    do {
      rv = write(_notify_send_fd, "", 1);
    } while (rv < 0 && errno == EAGAIN && ++cnt < 100);
  }

  void push_q_notify(int fd) {
    push_q.push(fd);
    /* 
    if (write(_push_send_fd, "", 1) != 1) {
      perror("Writing to thread notify pipe"); 
    } 
    */
    
    int rv, cnt = 0;
    do {
      rv = write(_push_send_fd, "", 1);
    } while (rv < 0 && errno == EAGAIN  && ++cnt < 100);
  }

  static void thread_libevent_process(int fd, short which, void *arg);
  static void thread_push_event_process(int fd, short which, void *arg);

public:
  LockQueue<cq_item> cq;     /* queue of new connections to handle */
  LockQueue<int>     push_q; /* queue of new push event to handle */

protected:
  int do_thread_func();

private:
  struct event_base *_base;    /* libevent handle this thread uses */
  struct event _notify_event;  /* listen event for notify pipe */
  int _notify_receive_fd;      /* receiving end of notify pipe */
  int _notify_send_fd;         /* sending end of notify pipe */

  struct event _push_event;  /* listen event for push pipe */
  int _push_receive_fd;      /* receiving end of push pipe */
  int _push_send_fd;         /* sending end of push pipe */
};

void thread_init();
void thread_stop();
void dispatch_conn_new(int sfd, enum conn_states init_state, int event_flags,
    const struct sockaddr_storage *addr);
void accept_new_conns(bool do_accept);

LibeventThread *get_main_thread();
LibeventThread* get_worker_thread(int i);

#endif /* __PS_THREAD_INCLUDE__ */
