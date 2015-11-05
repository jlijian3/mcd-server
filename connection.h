
/*
 * Copyright (C) jlijian3@gmail.com
 */

#ifndef __PS_CONNECTION_INCLUDE__
#define __PS_CONNECTION_INCLUDE__

#include "base_server.h"

enum conn_states {
  conn_listening,
  conn_new_req,
  conn_waiting,
  conn_read,
  conn_parse_req,
  conn_write,
  conn_closing,
  conn_unknown
};

enum conn_error {
  conn_ok,
  conn_wr_err,
  conn_rd_err,
  conn_reset_by_peer
};

enum write_buf_result {
  WRITE_COMPLETE,
  WRITE_INCOMPLETE,
  WRITE_SOFT_ERROR,
  WRITE_HARD_ERROR,
};


typedef struct conn conn;

class LibeventThread;

struct conn {
  int               fd;
  enum conn_states  state;
  enum conn_states  parse_to_go;
  enum conn_states  write_to_go;
  struct event      event;
  short             ev_flags;
  short             which;
  struct evbuffer  *rbuf;
  struct evbuffer  *wbuf;
 
  struct event      timeout_event;
  rel_time_t        active_time;

  int               keepalive;
  int               client_id;
  int               error; 
  void            (*close_callback)(conn *c);
  void            (*push_event_handler)(int, short, void *);
  void            (*write_callback)(conn *, enum write_buf_result, void *);
  void             *write_cb_arg;

  string           *host;
  unsigned short    port;
  LibeventThread   *thread;
  conn             *next;
};

enum try_parse_result {
  PARSE_OK,             /* parse ok */
  PARSE_NEED_MORE_DATA, /* need more data */
  PARSE_BAD_CLIENT,     /* client format error */
  PARSE_INNER_ERROR     /* server inner error */
};

typedef enum try_parse_result (*parse_request_pt)(conn *c);

void conn_init();
conn *conn_new(int fd, enum conn_states init_state,
               int event_flags, LibeventThread *thread);
void conn_close(conn *c);
void conn_free(conn *c);
void conn_set_state(conn *c, conn_states state);

conn *conn_from_fd(int fd);
void conn_thread_safe_op(int fd, void (*cb)(conn *, void *), void *arg);

bool update_event(conn *c, const int new_flags);

bool conn_push_data(conn *c, const char *data, int data_len);

bool conn_push_data(int fd, const char *data, int data_len);

bool conn_push_data(int fd, evbuffer *buf);

void conn_push_notify(conn *c);
void set_request_parser(parse_request_pt parser);

void conn_set_write_cb(conn *c,
    void (*cb)(conn *, enum write_buf_result, void *), void *arg);

int conn_fd_map_size();

#define CONN_LOG(_c, _level, _fmt, ...) \
  LOGGER->DebugInfo(_level, "fd:%d(%s:%d) " _fmt, _c->fd, _c->host->c_str(), _c->port, ##__VA_ARGS__)

#endif /* __PS_CONNECTION_INCLUDE__ */
