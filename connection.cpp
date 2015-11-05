
/*
 * Copyright (C) jlijian3@gmail.com
 * reference memcached source code
 */

#include <fcntl.h>
#include <errno.h>
#include <vector>
#include <map>

#include "connection.h"
#include "util.h"
#include "thread.h"
#include "log.h"

using namespace std;

static const char* state_names[] = {
  "conn_listening",
  "conn_new_req",
  "conn_waiting",
  "conn_read",
  "conn_parse_req",
  "conn_write",
  "conn_closing",
  "conn_unknown"
};

#define FREE_CONNS 200

static vector<conn *> *free_conns = NULL;
/* Lock for connection freelist */
static pthread_mutex_t freeconn_lock = PTHREAD_MUTEX_INITIALIZER;

static map<int, conn*> *fd_map = NULL;
static pthread_mutex_t fd_map_lock = PTHREAD_MUTEX_INITIALIZER;

static bool conn_add_to_freelist(conn *c);
static conn *conn_from_freelist();

static void conn_add_to_fd_map(int fd, conn *c);
static void conn_del_from_fd_map(int fd);

static void conn_cleanup(conn *c);

static void event_handler(int fd, short which, void *arg);
static void drive_machine(conn *c);

static void push_event_handler(int fd, short which, void *arg);

enum try_read_result {
  READ_DATA_RECEIVED,
  READ_NO_DATA_RECEIVED,
  READ_ERROR,            /** an error occured (on the socket) (or client closed connection) */
  READ_MEMORY_ERROR      /** failed to allocate more memory */
};

static enum try_read_result try_conn_read(conn *c);

static enum write_buf_result conn_write_buf(conn *c);

static volatile bool allow_new_conns = true;

static parse_request_pt default_request_parser;

enum try_parse_result dummy_parse_request(conn *c) {
  assert(c);

  int len = evbuffer_get_length(c->rbuf);
  evbuffer_drain(c->rbuf, len);
 
  evbuffer_add_printf(c->wbuf, "welcom to base server!");
  c->parse_to_go = conn_write;
  return PARSE_OK;
}

void set_request_parser(parse_request_pt parser) {
  default_request_parser = parser;
}

void conn_init() {
  set_request_parser(dummy_parse_request);
 
  if (!free_conns) {
    free_conns = new vector<conn *>();
    assert(free_conns);
  }

  if (!fd_map) {
    fd_map = new map<int, conn*>();
    assert(fd_map); 
  }

  for (int i = 0; i < FREE_CONNS; i++) {
    conn *c = (conn *)calloc(1, sizeof(conn));
    
    if (c == NULL) {
      perror("conn_init calloc()");  
      continue;
    }

    c->rbuf = evbuffer_new();
    c->wbuf = evbuffer_new();
    
    if (NULL == c->rbuf ||
        NULL == c->wbuf ||
        evbuffer_enable_locking(c->wbuf, NULL) != 0) {
      conn_free(c);
      dlog1("evbuffer_new error or enable locking error\n");
      continue;
    }

    free_conns->push_back(c); 
  }

  return;
}

conn *conn_new(int sfd, enum conn_states init_state,
               int event_flags, LibeventThread *thread) {
  assert(thread);

  struct event_base *base = thread->get_event_base();
  conn *c = conn_from_freelist();
 
  if (NULL == c) {
    if (!(c = (conn *)calloc(1, sizeof(conn)))) {
      dlog1("conn_new calloc error\n");
      return NULL;
    }
    
    c->rbuf = evbuffer_new();
    c->wbuf = evbuffer_new();

    if (NULL == c->rbuf ||
        NULL == c->wbuf ||
        evbuffer_enable_locking(c->wbuf, NULL) != 0 ) {
      conn_free(c);
      dlog1("evbuffer_new error or enable locking error\n");
      return NULL;
    }
  } else {
    evbuffer_drain(c->rbuf, evbuffer_get_length(c->rbuf)); 
    evbuffer_drain(c->wbuf, evbuffer_get_length(c->wbuf));  
  }

  if (!c->host)
    c->host = new string();

  c->thread = thread;
  c->push_event_handler = push_event_handler; 
  c->active_time = current_time;
  c->fd = sfd;
  c->state = init_state;
  c->parse_to_go = conn_unknown;
  c->write_to_go = conn_unknown;

  event_set(&c->event, sfd, event_flags, event_handler, (void *)c);
  event_base_set(base, &c->event);
  c->ev_flags = event_flags;

  if (event_add(&c->event, NULL) == -1) {
    if (!conn_add_to_freelist(c)) {
      conn_free(c);
    }
    perror("event_add");
    return NULL;
  }
 
  conn_add_to_fd_map(c->fd, c);
  return c;
}

static void conn_cleanup(conn *c) {
  assert(c);
  c->fd = 0;
  c->client_id = 0;
  c->keepalive = 0;
  c->error = conn_ok;  
  c->close_callback = NULL;
  c->write_callback = NULL;
  c->write_cb_arg = NULL;
  c->ev_flags = 0;
  c->thread = NULL; 
  c->push_event_handler = NULL; 
  c->next = NULL;
  c->host->clear();
  c->port = 0;
  evbuffer_drain(c->rbuf, evbuffer_get_length(c->rbuf)); 
  evbuffer_drain(c->wbuf, evbuffer_get_length(c->wbuf));
}

void conn_free(conn *c) {
  if (c) {
    if (c->rbuf)
      evbuffer_free(c->rbuf);
    if (c->wbuf)
      evbuffer_free(c->wbuf);
    if (c->host)
      delete c->host;
    free(c);
  }
}

void conn_close(conn *c) {
  if (c == NULL) {
    dlog4("conn_close c == NULL\n");
    return;
  }

  dlog4("conn_close conn fd:%d, (%s:%d)\n", c->fd, c->host->c_str(), c->port);

  conn_del_from_fd_map(c->fd);
  
  if (c->close_callback)
    c->close_callback(c); 

  event_del(&c->event);
  close(c->fd);
   
  conn_cleanup(c);
  if (!conn_add_to_freelist(c)) {
    conn_free(c); 
  }

  if (!allow_new_conns) {
    allow_new_conns = true;
    accept_new_conns(true);
  }
}

void conn_thread_safe_op(int fd, void (*cb)(conn *, void *), void *arg) {
  conn *c = NULL;
  map<int, conn *>::iterator it;

  if (pthread_mutex_lock(&fd_map_lock) != 0)
    return;

  it = fd_map->find(fd);
  if (it != fd_map->end()) {
    c = it->second;
  }

  cb(c, arg);
  pthread_mutex_unlock(&fd_map_lock);
}

void conn_set_state(conn *c, conn_states state) {
  assert(c);
  
  if (state != c->state) {
    c->state = state; 
  }
}

static conn *conn_from_freelist() {
  conn *c = NULL;
 
  if (pthread_mutex_lock(&freeconn_lock) != 0)
    return c;
  
  if (!free_conns->empty()) {
    c = free_conns->back();
    free_conns->pop_back();
  }
  dlog4("conn_from_freelist free conns:%lu\n", free_conns->size());
  pthread_mutex_unlock(&freeconn_lock);

  return c; 
}

static bool conn_add_to_freelist(conn *c) {
  assert(c);

  if (pthread_mutex_lock(&freeconn_lock) != 0)
    return false;

  free_conns->push_back(c);
  pthread_mutex_unlock(&freeconn_lock); 
  
  return true;
}

conn *conn_from_fd(int fd) {
  map<int, conn *>::iterator it;
  conn *c = NULL;

  if (pthread_mutex_lock(&fd_map_lock) != 0)
    return NULL;

  it = fd_map->find(fd);
  if (it != fd_map->end())
    c = it->second;

  pthread_mutex_unlock(&fd_map_lock);
  return c;
}

static void conn_add_to_fd_map(int fd, conn *c) {
  if (pthread_mutex_lock(&fd_map_lock) != 0)
    return;

  fd_map->insert(pair<int, conn *>(fd, c));

  dlog4("conn_add_to_fd_map size:%lu\n", fd_map->size());
  pthread_mutex_unlock(&fd_map_lock);
}

static void conn_del_from_fd_map(int fd) {
  map<int, conn *>::iterator it; 
  
  if (pthread_mutex_lock(&fd_map_lock) != 0)
    return;
  
  it = fd_map->find(fd);
  if (it != fd_map->end())
    fd_map->erase(it);
 
  dlog4("conn_del_from_fd_map size:%lu\n", fd_map->size());
  pthread_mutex_unlock(&fd_map_lock);
}

int conn_fd_map_size() {
  int size;

  if (pthread_mutex_lock(&fd_map_lock) != 0)
    return -1;
  
  size = fd_map->size();
  pthread_mutex_unlock(&fd_map_lock);
  
  return size;
}

bool update_event(conn *c, const int new_flags) {
  assert(c);

  struct event_base *base = c->event.ev_base;
  
  if (c->ev_flags == new_flags)
    return true;
  if (event_del(&c->event) == -1)
    return false;
 
  event_set(&c->event, c->fd, new_flags, event_handler, (void *)c);
  event_base_set(base, &c->event);
  c->ev_flags = new_flags;
  if (event_add(&c->event, NULL) == -1)
    return false;
  return true;
}

static void reset_req_handler(conn *c) {
  size_t buflen = evbuffer_get_length(c->rbuf);
  
  if (buflen > 0) {
    conn_set_state(c, conn_parse_req);
  } else {
    conn_set_state(c, conn_waiting); 
  }
}

static enum try_read_result try_conn_read(conn *c) {
  assert(c);
  enum try_read_result gotdata;
  int nread;

  gotdata = evbuffer_get_length(c->rbuf) ?
      READ_DATA_RECEIVED : READ_NO_DATA_RECEIVED;

  while (1) {
    nread = evbuffer_read(c->rbuf, c->fd, DATA_BUFFER_SIZE);
    if (nread > 0) {
      gotdata = READ_DATA_RECEIVED;
      if (nread == DATA_BUFFER_SIZE) {
        continue;
      } else {
        break;
      }
    }
   
    if (nread == 0) {
      c->error = conn_reset_by_peer; 
      return READ_ERROR;
    }

    if (nread == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break; 
      }
      dlog1("evbuffer_read(): %s\n", strerror(errno)); 
      c->error = conn_rd_err;
      return READ_ERROR;
    }
  }

  return gotdata;
}

static enum write_buf_result conn_write_buf(conn *c) {
  assert(c);
  int nwrite;
  int wsize = evbuffer_get_length(c->wbuf);
  enum write_buf_result rv;

  do {
    if (wsize == 0) {
      rv = WRITE_COMPLETE;
      break; 
    }

    /* ?ֶ?Ч?ʵ? 
    if (wsize > DATA_BUFFER_SIZE)
      nwrite = evbuffer_write_atmost(c->wbuf, c->fd, DATA_BUFFER_SIZE); 
    else */
      nwrite = evbuffer_write(c->wbuf, c->fd);
    
    dlog4("conn_write_buf fd:%d, wsize:%d, nwrite:%d\n",
          c->fd, wsize, nwrite);
    
    if (nwrite > 0) {
      if (nwrite < wsize)
        rv = WRITE_INCOMPLETE;
      else
        rv = WRITE_COMPLETE;
      break; 
    }

    if (nwrite == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      dlog1("evbuffer_write(): %s\n", strerror(errno));
      if (!update_event(c, EV_WRITE | EV_PERSIST)) {
        dlog4("Couldn't update event\n");
        c->error = conn_wr_err; 
        rv = WRITE_HARD_ERROR;
      } else {
        rv = WRITE_SOFT_ERROR;
      }
      break;
    }
    
    c->error = conn_wr_err;
    rv = WRITE_HARD_ERROR;
  } while (0);
 
  if (c->write_callback) {
    c->write_callback(c, rv, c->write_cb_arg);
    conn_set_write_cb(c, NULL, NULL); 
  }
  return rv;
}


static void event_handler(int fd, short which, void *arg) {
  conn *c = (conn *)arg;

  assert(c);

  c->which = which;
  c->active_time = current_time;

  if (fd != c->fd) {
    dlog4("event_handler:event fd != conn->fd!\n");
    conn_close(c);
    return;
  }


  drive_machine(c);
}

static void drive_machine(conn *c) {
  bool      stop = false;
  int       sfd, flags = 1;
  socklen_t addrlen;
  struct sockaddr_storage addr;
  int res;
  int nreqs = base_conf.nreqs_per_event;
   
  assert(c);

  int istimeout = c->which&EV_TIMEOUT;
  
  while (!stop) {
    dlog4("conn fd:%d,which:%d,state:%s,keepalive:%d\n",
          c->fd, c->which, state_names[c->state], c->keepalive); 
    
    switch (c->state) {
    
    case conn_listening:
      addrlen = sizeof(addr);
      if ((sfd = accept(c->fd, (struct sockaddr *)&addr, &addrlen)) == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          /* these are transient, so don't log anything */
          perror("accept()");
          stop = true;
        } else if (errno == EMFILE) {
          dlog4("Too many open connections\n");
          accept_new_conns(false);
          allow_new_conns = false; 
          stop = true;
        } else {
          perror("accept()");
          stop = true;
        }
        break;
      }

      if (fd_map->size() > (size_t)base_conf.max_conns) {
        dlog4("Too many open connections:%d\n", base_conf.max_conns);
        close(sfd);
        accept_new_conns(false);
        allow_new_conns = false; 
        stop = true;
        break;
      }

      if ((flags = fcntl(sfd, F_GETFL, 0)) < 0 ||
        fcntl(sfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("setting O_NONBLOCK");
        close(sfd);
        break;
      }
      
      dispatch_conn_new(sfd, conn_new_req, EV_READ | EV_PERSIST, &addr);
      stop = true;
      break;

    case conn_read:
      res = try_conn_read(c);
       
      dlog4("try_conn_read fd:%d, gotdata:%d\n", c->fd, res); 

      switch (res) {
      case READ_NO_DATA_RECEIVED:
        if (istimeout && !c->keepalive)
          conn_set_state(c, conn_closing);
        else
          conn_set_state(c, conn_waiting);
        break;
      case READ_DATA_RECEIVED:
        conn_set_state(c, conn_parse_req);
        break;
      case READ_ERROR:
        conn_set_state(c, conn_closing);
        break;
      case READ_MEMORY_ERROR: /* Failed to allocate more memory */
        break;  
      }
      break;

    case conn_waiting:
      if (!update_event(c, EV_READ | EV_PERSIST)) {
        dlog4("update event failed\n");
        conn_set_state(c, conn_closing);
        break;
      }

      conn_set_state(c, conn_read);
      stop = true;
      break;

    case conn_new_req:
      if (--nreqs >= 0) {
        reset_req_handler(c);
      } else {
        if (evbuffer_get_length(c->rbuf) > 0) {
          if (!update_event(c, EV_WRITE | EV_PERSIST)) {
            dlog4("couldn't update event\n");
            conn_set_state(c, conn_closing);
          }
        }
        stop = true;
      }
      break;

    case conn_parse_req:
      switch (default_request_parser(c)) {
      case PARSE_NEED_MORE_DATA:
        conn_set_state(c, conn_waiting);
        break;
      
      case PARSE_BAD_CLIENT:
        conn_set_state(c, conn_closing);
        break;

      case PARSE_INNER_ERROR:
        conn_set_state(c, conn_new_req);
        break; 
      
      case PARSE_OK:
        if (c->parse_to_go != conn_unknown)
          conn_set_state(c, c->parse_to_go);
        else {
          conn_set_state(c, c->keepalive ? conn_new_req : conn_closing);
        }
        break;
      }
      break;

    case conn_write:
      {
        enum write_buf_result rv = conn_write_buf(c);
        dlog4("event_handler fd:%d conn_write_buf result:%d\n", c->fd, rv);
        switch (rv) {
        case WRITE_COMPLETE:
          if (c->write_to_go != conn_unknown) {
            dlog4("conn fd:%d c->write_to_go:%d\n", c->fd, c->write_to_go);
            conn_set_state(c, c->write_to_go);
            c->write_to_go = conn_unknown;
          } else {
            conn_set_state(c, c->keepalive ? conn_new_req : conn_closing);
          } 
          break;

        case WRITE_INCOMPLETE:
          break; /* continue in state machine */

        case WRITE_HARD_ERROR:
          conn_set_state(c, conn_closing);
          break;

        case WRITE_SOFT_ERROR:
          stop = true;
          break;
        }
        break;
      }

    case conn_closing:
      conn_close(c);
      stop = true;
      break;

    case conn_unknown:
      /* (assert(0); */
      dlog1("conn fd:%d, drive_machine conn_state unknow\n", c->fd);
      conn_close(c); 
      stop = true; 
      break;
    
    } /* switch (c->state) */
  } /* while (!stop) */

  return;
}

bool conn_push_data(conn *c, const char* data, int data_len) {
  assert(c);

  bool  rv = false;

  if (c) {
    rv = evbuffer_add(c->wbuf, data, data_len);
    if (rv)
      conn_push_notify(c);
  }

  return rv;
}

bool conn_push_data(int fd, const char* data, int data_len) {
  map<int, conn *>::iterator it; 
  conn *c  = NULL;
  bool  rv = false;

  if (pthread_mutex_lock(&fd_map_lock) != 0)
    return false;

  it = fd_map->find(fd);
  if (it != fd_map->end())
    c = it->second;
 
  if (c) {
    rv = evbuffer_add(c->wbuf, data, data_len);
    if (rv) 
      conn_push_notify(c);
  }

  pthread_mutex_unlock(&fd_map_lock);

  return rv;
}

bool conn_push_data(int fd, evbuffer *buf) {
  assert(buf);
  
  map<int, conn *>::iterator it; 
  conn *c  = NULL;
  int buflen = evbuffer_get_length(buf);

  if (buflen == 0)
    return false;

  if (pthread_mutex_lock(&fd_map_lock) != 0)
    return false;

  it = fd_map->find(fd);
  if (it != fd_map->end())
    c = it->second;
  
  if (c) {
    evbuffer_remove_buffer(buf, c->wbuf, buflen);
    conn_push_notify(c);
  }

  pthread_mutex_unlock(&fd_map_lock);

  return true;
}

void conn_push_notify(conn *c) {
  assert(c);

  dlog4("conn_push_notify fd:%d\n", c->fd);
  
  c->thread->push_q_notify(c->fd);
}

static void push_event_handler(int fd, short which, void *arg) {
  conn *c = (conn *)arg;
  enum write_buf_result rv;

  assert(c);

  if (c->state == conn_write)
    return;

  while (1) {
    rv = conn_write_buf(c);
    dlog4("push_event_handler conn fd:%d,which:%d,conn_write_buf result:%d\n",
          c->fd, c->which, rv);

    if (WRITE_INCOMPLETE == rv)
      continue;
    
    if (WRITE_SOFT_ERROR == rv) {
      c->write_to_go = c->keepalive ? c->state : conn_closing; 
      conn_set_state(c, conn_write);
      break; 
    }

    if (c->keepalive == 0)
      conn_close(c);
    break;
  }
}

void conn_set_write_cb(conn *c,
    void (*cb)(conn *, enum write_buf_result, void *),
    void *arg)
{
  assert(c);

  c->write_callback = cb;
  c->write_cb_arg = arg;
}

