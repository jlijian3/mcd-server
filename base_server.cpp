
/*
 * Copyright (C) jlijian3@gmail.com
 * date: 2011-08
 * reference memcached source code
 */

#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <time.h>

#include "base.h"
#include "base_server.h"
#include "connection.h"
#include "setup.h"
#include "log.h"
#include "thread.h"

struct event_base *main_base;
struct base_conf_t base_conf;

static conn *listen_conn;
static volatile bool listen_disable = false;

volatile rel_time_t current_time;
static struct event clockevent;
static void (*clock_callback)(rel_time_t);

static void set_current_time(void) {
  struct timeval timer;

  gettimeofday(&timer, NULL);
  current_time = (rel_time_t)timer.tv_sec;
}

static void clock_handler(const int fd, const short which, void *arg) {
  struct timeval t;
  static bool initialized = false;
  
  if (initialized) {
    /* only delete the event if it's actually there. */
    evtimer_del(&clockevent);
  } else {
    initialized = true;
  }

  t.tv_sec = 1;
  t.tv_usec = 0;
  evtimer_set(&clockevent, clock_handler, 0);
  event_base_set(main_base, &clockevent);
  evtimer_add(&clockevent, &t);

  set_current_time();
  if (clock_callback)
    clock_callback(current_time);
}

static int new_socket(struct addrinfo *ai) {
  int sfd;
  int flags;

  if ((sfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) == -1) {
    return -1;
  }

  if ((flags = fcntl(sfd, F_GETFL, 0)) < 0 ||
    fcntl(sfd, F_SETFL, flags | O_NONBLOCK) < 0) {
    perror("setting O_NONBLOCK");
    close(sfd);
    return -1;
  }
  return sfd;
}

int server_socket(const char *interface, int port, int backlog) {
  int sfd;
  struct linger ling;
  struct addrinfo *ai;
  struct addrinfo *next;
  struct addrinfo hints;
  char port_buf[NI_MAXSERV];
  int error;
  int success = 0;
  int flags = 1;

  ling.l_onoff = 0;
  ling.l_linger = 0;
  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = base_conf.support_ipv6 ? AF_UNSPEC : AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (port == -1) {
    port = 0;
  }
  
  snprintf(port_buf, sizeof(port_buf), "%d", port);
  error= getaddrinfo(interface, port_buf, &hints, &ai);
  if (error != 0) {
    if (error != EAI_SYSTEM)
      fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(error));
    else
      perror("getaddrinfo()");
    return 1;
  }
  
  for (next= ai; next; next= next->ai_next) {
    conn *listen_conn_add;
    if ((sfd = new_socket(next)) == -1) {
      close(sfd);
      freeaddrinfo(ai);
      return 1; 
    }

#ifdef IPV6_V6ONLY
    if (next->ai_family == AF_INET6) {
      error = setsockopt(sfd, IPPROTO_IPV6, IPV6_V6ONLY,
                         (char *) &flags, sizeof(flags));
      if (error != 0) {
        perror("setsockopt");
        close(sfd);
        continue;
      }
    }
#endif

    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));

    error = setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&flags,
                       sizeof(flags));
    if (error != 0)
      perror("setsockopt");

    error = setsockopt(sfd, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));
    if (error != 0)
      perror("setsockopt");

    error = setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, (void *)&flags,
                       sizeof(flags));
    if (error != 0)
      perror("setsockopt");

    if (bind(sfd, next->ai_addr, next->ai_addrlen) == -1) {
      perror("bind()");
      close(sfd);
      freeaddrinfo(ai);
      return 1;
    } else {
      success++;
      if (listen(sfd, backlog) == -1) {
        perror("listen()");
        close(sfd);
        freeaddrinfo(ai);
        return 1;
      }
    }

    if (!(listen_conn_add = conn_new(sfd, conn_listening,
                                     EV_READ | EV_PERSIST, get_main_thread()))) {
      fprintf(stderr, "failed to create listening connection\n");
      exit(EXIT_FAILURE);
    }
    
    listen_conn_add->host->assign(interface?interface:"0.0.0.0");
    listen_conn_add->port = port;
    listen_conn_add->next = listen_conn;
    listen_conn = listen_conn_add;
  }

  freeaddrinfo(ai);
  return success == 0;
}

void do_accept_new_conns(bool do_accept) {
  if (do_accept != !listen_disable)
    return;
  
  conn *next;

  for (next = listen_conn; next; next = next->next) {
    if (do_accept) {
      update_event(next, EV_READ | EV_PERSIST);
      if (listen(next->fd, base_conf.listen_backlog) != 0) {
        perror("listen");
      }
    } else {
      update_event(next, 0);
      if (listen(next->fd, 0) != 0) {
        perror("listen");
      }
    }
  }

  listen_disable = !do_accept;
}

void base_server_init(const Setup *setup) {
  base_conf_init(setup);
  /*cout << "event method: " << event_base_get_method(main_base) << endl;*/
  evthread_use_pthreads();
  thread_init();
  main_base = get_main_thread()->get_event_base();
  conn_init();
  clock_handler(0, 0, 0);
}

void base_server_loop() {
  event_base_loop(main_base, 0);
}

void base_server_stop() {
  if (listen_conn)
    conn_close(listen_conn);
  if (main_base) 
    event_base_loopbreak(main_base);
}

struct event_base *get_main_base() {
  return main_base;
}

void set_clock_callback(void (*cb)(rel_time_t), void (**old_cb)(rel_time_t)) {
  assert(cb);
  *old_cb = clock_callback;
  clock_callback = cb;
}

void base_conf_init(const Setup *setup) {
  base_conf.nthreads = setup->MAX_CMD_THREAD_NUM;
  base_conf.nreqs_per_event = setup->REQS_PER_EVENT;
  base_conf.listen_backlog = setup->LISTEN_QUE_SIZE;
  base_conf.support_ipv6 = setup->SUPPORT_IPV6;
  base_conf.max_conns = setup->MAX_CONNS;
}
