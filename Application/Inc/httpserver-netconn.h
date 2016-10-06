#ifndef __HTTPSERVER_NETCONN_H__
#define __HTTPSERVER_NETCONN_H__

#include"lwip/api.h"

void http_server_netconn_init(void);
void ReturnTasks(struct netconn *conn);
void ReturnADC(struct netconn *conn);
void ReturnLeds(struct netconn *conn, char* buf);

#endif /* __HTTPSERVER_NETCONN_H__ */
