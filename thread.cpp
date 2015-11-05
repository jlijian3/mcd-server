
/*
 * Copyright (C) jlijian3@gmail.com
 * reference memcached source code
 */

#include <pthread.h>
#include <vector>

#include "util.h"
#include "thread.h"
#include "log.h"

using namespace std;

/* Connection lock around accepting new connections */
pthread_mutex_t conn_lock = PTHREAD_MUTEX_INITIALIZER;

static vector<LibeventThread*> threads;
static LibeventThread dispatch_thread;

/*
 * Number of worker threads that have finished setting themselves up.
 */
static int init_count;
static pthread_mutex_t init_lock;
static pthread_cond_t init_cond;

bool LibeventThread::init() {
  int fds[2];
  if (pipe(fds)) {
      perror("Can't create notify pipe");
      return false;
  }

  _notify_receive_fd = fds[0];
  _notify_send_fd = fds[1];
  evutil_make_socket_nonblocking(_notify_receive_fd);
  evutil_make_socket_nonblocking(_notify_send_fd);

  if (pipe(fds)) {
    perror("Can't create notify pipe");
    return false;
  }

  _push_receive_fd = fds[0];
  _push_send_fd = fds[1];
  evutil_make_socket_nonblocking(_push_receive_fd);
  evutil_make_socket_nonblocking(_push_send_fd);
  
  _base = event_init(); 
  if (!_base) {
    dlog4("Can't allocate event base\n");
    return false;
  }

  /* Listen for notifications from other threads */
  event_set(&_notify_event, _notify_receive_fd,
            EV_READ | EV_PERSIST, thread_libevent_process, this);
  event_base_set(_base, &_notify_event);

  if (event_add(&_notify_event, 0) == -1) {
    dlog4("Can't monitor libevent notify pipe\n");
    return false;
  }

  /* Listen for notifications from other connections */
  event_set(&_push_event, _push_receive_fd,
            EV_READ | EV_PERSIST, thread_push_event_process, this);
  event_base_set(_base, &_push_event);

  if (event_add(&_push_event, 0) == -1) {
    dlog4("Can't monitor libevent push pipe\n");
    return false;
  }

  return true;
}

bool LibeventThread::stop() {
  if (!_base)
    return false;

  return 0 == event_base_loopbreak(_base);
}

int LibeventThread::do_thread_func() {

  pthread_mutex_lock(&init_lock);
  init_count++;
  pthread_cond_signal(&init_cond);
  pthread_mutex_unlock(&init_lock);
  
  event_base_loop(this->get_event_base(), 0);
  return 0;
}

void LibeventThread::thread_libevent_process(int fd,
                                             short which,
                                             void *arg) {
  LibeventThread *me = (LibeventThread*)arg; 
  cq_item item;
  char buf[1024];

  /*
  if (read(fd, buf, 1) != 1) {
    dlog4("Can't read from libevent pipe\n");
  }
  */

  while (read(fd, buf, sizeof(buf)) > 0)
    ;
 
  while (1) {
    try {
      item = me->cq.pop();
      conn *c = conn_new(item.sfd, item.init_state, item.event_flags,
                         me);
      if (c == NULL) {
        dlog4("Can't listen for events on fd %d\n", item.sfd);
        close(item.sfd); 
      }
      
      char ntop[NI_MAXHOST];
      char strport[NI_MAXSERV];
      int ni_rv;

      ni_rv = getnameinfo((struct sockaddr *)&item.addr, sizeof(item.addr),
          ntop, sizeof(ntop), strport, sizeof(strport),
          NI_NUMERICHOST|NI_NUMERICSERV);
   
      if (ni_rv != 0) {
        perror("getnameinfo");
        ntop[0] = 0;
        strport[0] = 0;
      } else {
        c->host->assign(ntop);
        c->port = atoi(strport);
      }
      
      dlog4("conn_new conn fd:%d, (%s:%s) cq:%lu\n", item.sfd, ntop, strport, me->cq.size());
      
    } catch (const std::exception e) {
      break;
    }
  }
}

void LibeventThread::thread_push_event_process(int fd,
                                               short which,
                                               void *arg) {
  LibeventThread *me = (LibeventThread*)arg; 
  int  conn_fd;
  char buf[1024];

  /*
  if (read(fd, buf, 1) != 1) {
    dlog4("Can't read from libevent pipe\n");
  }
  */

  while (read(fd, buf, sizeof(buf)) > 0)
    ;

  while (1) {
    try {
      conn_fd = me->push_q.pop();
      conn *c = conn_from_fd(conn_fd);
      if (c) {
        c->push_event_handler(fd, which, (void*)c);
      } else {
        dlog4("push conn fd %d is closed\n", conn_fd);
      }
    } catch (const std::exception e) {
      break;
    }
  }
}

void thread_init() {

  pthread_mutex_init(&init_lock, NULL);
  pthread_cond_init(&init_cond, NULL); 
 
  if (!dispatch_thread.init())
    exit(1);

  for (int i = 0; i < base_conf.nthreads; i++) {
    LibeventThread *thread = new LibeventThread();
    if (!thread->init())
      exit(1);
    thread->create(); 
    threads.push_back(thread);     
  }

  /* Wait for all the threads to set themselves up before returning. */
  pthread_mutex_lock(&init_lock);
  while (init_count < base_conf.nthreads) {
      pthread_cond_wait(&init_cond, &init_lock);
  }
  pthread_mutex_unlock(&init_lock);
}

void thread_stop() {
  
  for (size_t i = 0; i < threads.size(); i++) {
    LibeventThread *thread = threads[i];   
    thread->stop();
    thread->wait();
    delete thread;
  }

  threads.clear();
}

static int last_thread = -1;

void dispatch_conn_new(int sfd,
                       enum conn_states init_state,
                       int event_flags,
                       const struct sockaddr_storage *addr) {
  cq_item item(sfd, init_state, event_flags);
 
  if (addr)
    item.addr = *addr;

  int tid = (last_thread + 1) % base_conf.nthreads;

  LibeventThread *thread = threads[tid];
  
  last_thread = tid;

  thread->cq.push(item);
  thread->cq_notify();
}

/*
 * Sets whether or not we accept new connections.
 */
void accept_new_conns(bool do_accept) {
    pthread_mutex_lock(&conn_lock);
    do_accept_new_conns(do_accept);
    pthread_mutex_unlock(&conn_lock);
}

LibeventThread *get_main_thread() {
  return &dispatch_thread;
}

LibeventThread* get_worker_thread(int i) {
  if ((size_t)i < threads.size())
    return threads[i];
  return NULL;
}
