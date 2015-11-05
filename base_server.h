
/*
 * Copyright (C) jlijian3@gmail.com
 */

#ifndef __BASE_SERVER_INCLUDE__
#define __BASE_SERVER_INCLUDE__

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <event.h>
#include <event2/thread.h>
#include <iostream>

#include "setup.h"

#define DATA_BUFFER_SIZE 2048
#define BASE_INT64_LEN   sizeof("-9223372036854775808") - 1

typedef unsigned int rel_time_t;

struct base_conf_t {
  int nthreads;
  int nreqs_per_event;
  int listen_backlog;
  int support_ipv6;
  int max_conns;
};

void base_server_init(const Setup *settings);
void base_conf_init(const Setup *settings);
void base_server_loop();
void base_server_stop();

void do_accept_new_conns(bool do_accept);
void set_clock_callback(void (*cb)(rel_time_t), void (**old_cb)(rel_time_t));

struct event_base *get_main_base();

int server_socket(const char *interface, int port, int backlog);

extern volatile rel_time_t current_time;
extern struct base_conf_t  base_conf;
#endif /* __BASE_SERVER_INCLUDE__ */
