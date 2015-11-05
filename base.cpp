
/*
 * Copyright (C) jlijian3@gmail.com
 */

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "base.h"

void BaseThread::create() {
  pthread_attr_t  attr;
  int             ret;

  pthread_attr_init(&attr);

  if ((ret = pthread_create(&_thread_id, &attr,
                            thread_func, (void *)this)) != 0) {
    fprintf(stderr, "Can't create thread: %s\n", strerror(ret));
    exit(1);
  }
}

void *BaseThread::thread_func(void *arg) {
  assert(arg);
  int rv;

  BaseThread *me = (BaseThread *)arg;
  rv = me->do_thread_func();
  pthread_exit((void *)&rv);
}


