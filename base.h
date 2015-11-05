
/*
 * Copyright (C) jlijian3@gmail.com
 */

#ifndef __BASE_INCLUDE__
#define __BASE_INCLUDE__

#include <pthread.h>

class BaseThread {

public:
  BaseThread() {
  }

  virtual ~BaseThread() {
  }

  void create();
  
  int wait() {
    return pthread_join(_thread_id, NULL);
  }

  virtual bool stop() {
    return true; 
  };
  
  pthread_t get_tid() {
    return _thread_id;
  }

protected:
  virtual int do_thread_func() = 0;
  static void *thread_func(void *arg);

private:
  pthread_t _thread_id;
};

class ThreadCond {
public:
  ThreadCond() {
  }

  ~ThreadCond() {
    destroy(); 
  }
  
  bool init() {
    if (pthread_mutex_init(&_cond_lock, NULL) != 0)
      return false;
    if (pthread_cond_init(&_cond, NULL) != 0)
      return false;
    return true;
  }

  void destroy() {
    pthread_mutex_destroy(&_cond_lock);
    pthread_cond_destroy(&_cond); 
  }
 
  void wait() {
    pthread_mutex_lock(&_cond_lock);
    pthread_cond_wait(&_cond, &_cond_lock);
    pthread_mutex_unlock(&_cond_lock);  
  }

  void notify() {
    pthread_mutex_lock(&_cond_lock);
    pthread_cond_signal(&_cond);
    pthread_mutex_unlock(&_cond_lock);
  }

private: 
  pthread_mutex_t _cond_lock;
  pthread_cond_t _cond;
};

#endif
