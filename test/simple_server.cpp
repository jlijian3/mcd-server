#include <signal.h>

#include "base_core.h"

static Setup settings;

static enum try_parse_result simple_parse_requset(conn *c) {
  int len = evbuffer_get_length(c->rbuf);
  const char* buf = (const char*)evbuffer_pullup(c->rbuf, len);
  if (strncmp(buf, "quit", 4) == 0)
    c->keepalive = 0;
  else
    c->keepalive = 1;

  /* c->rbuf will be drained */
  evbuffer_add_buffer(c->wbuf, c->rbuf);
  /*evbuffer_drain(c->rbuf, len);*/
  c->parse_to_go = conn_write;
  return PARSE_OK;
}

static const char *http_resp = "HTTP/1.1 200 OK\r\n"
                               "Content-Type: text/plain\r\n"
                               "Content-Length: 7\r\n"
                               "\r\n"
                               "hello\r\n";

static enum try_parse_result http_parse_requset(conn *c) {
  c->keepalive = 0;
  int len = evbuffer_get_length(c->rbuf);
  evbuffer_add(c->wbuf, http_resp, strlen(http_resp));
  evbuffer_drain(c->rbuf, len);
  c->parse_to_go = conn_write;
  return PARSE_OK;
}

void handle_exit(int sig) { fprintf(stderr, "catch signal %d\n", sig);
  exit(0);
}

int main(int argc, char **argv)
{
  bool isprint = false, isflush = false;
	int debug_level = 0;
	string setup_file_path = "./setup.txt";
  string signame;

  GetOpt(argc, argv, isprint, isflush, debug_level, setup_file_path,
         signame, "simple_srv", "1.0");

  if (!settings.Load(setup_file_path)) {
    cerr << "load setup file error: " << setup_file_path << endl;
    exit(1);
  }  

  dlog1("simple server start...\n");

  base_server_init(&settings);

  set_request_parser(simple_parse_requset); 
  //set_request_parser(http_parse_requset);


  signal(SIGINT, handle_exit);
  signal(SIGTERM, handle_exit);
  signal(SIGKILL, handle_exit);
  signal(SIGQUIT, handle_exit);
  signal(SIGHUP, SIG_IGN);
   
  if (server_socket(NULL,
                    settings.LISTEN_PORT,
                    settings.LISTEN_QUE_SIZE))
  {
    vperror("failed listen on tcp port %d", settings.LISTEN_PORT);
    exit(1);
  }

  dlog1("listen port %d ...\n", settings.LISTEN_PORT);
  
  PIDSaveToFile(settings.PID_FILE_PATH);

  base_server_loop();
  
  return 0;
}
