/* -*- Mode: C; -*- */

/* Copyright (C) 2004-2012 beingmeta, inc.
   This file is part of the libu8 UTF-8 unicode library.

   This program comes with absolutely NO WARRANTY, including implied
   warranties of merchantability or fitness for any particular
   purpose.

    Use, modification, and redistribution of this program is permitted
    under any of the licenses found in the the 'licenses' directory 
    accompanying this distribution, including the GNU General Public License
    (GPL) Version 2 or the GNU Lesser General Public License.
*/

#include "libu8/libu8.h"

#ifndef _FILEINFO
#define _FILEINFO __FILE__
#endif

#include "libu8/libu8io.h"
#include "libu8/u8filefns.h"
#include "libu8/u8netfns.h"
#include "libu8/u8srvfns.h"
#include "libu8/u8timefns.h"
#include <unistd.h>
#include <limits.h>
#if HAVE_POLL_H
#include <poll.h>
#elif HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

#ifndef POLLRDHUP
#define POLLRDHUP 0
#endif

static u8_condition ClosedClient=_("Closed connection");
static u8_condition IdleClient=_("Idle Client");
static u8_condition ServerShutdown=_("Shutting down server");
static u8_condition NewServer=_("New server port");
static u8_condition NewClient=_("New connection");
static u8_condition RejectedConnection=_("Rejected connection");
static u8_condition ClientRequest=_("Client request");
static u8_condition BadSocket=_("Bad socket value");
static u8_condition Inconsistency=_("Internal inconsistency");

/* Prototypes for some static definitions */

static int client_close_core(u8_client cl,int server_locked);
static int finish_closing_client(u8_client cl);

static void update_client_stats(u8_client cl,long long cur,int done);
static void update_server_stats(u8_client cl);

/* Sockaddr functions */

static struct sockaddr *get_sockaddr_in
  (int family,int port,char *addr,int addr_len)
{
  if (family==AF_INET) {
    struct sockaddr_in *sockaddr=u8_alloc(struct sockaddr_in);
    memset(sockaddr,0,sizeof(struct sockaddr_in)); 
    sockaddr->sin_port=htons((short)port);
    sockaddr->sin_family=family;
    memcpy((char *)&(sockaddr->sin_addr),addr,addr_len);
    return (struct sockaddr *)sockaddr;}
#ifdef AF_INET6
  else if (family==AF_INET6) {
    struct sockaddr_in6 *sockaddr=u8_alloc(struct sockaddr_in6);
    memset(sockaddr,0,sizeof(struct sockaddr_in6)); 
    sockaddr->sin6_port=htons((short)port);
    sockaddr->sin6_family=family;
    memcpy((char *)&(sockaddr->sin6_addr),addr,addr_len);
    return (struct sockaddr *)sockaddr;}
#endif
  else return NULL;
}

static struct sockaddr *get_sockaddr_file(u8_string string)
{
#if HAVE_SYS_UN_H
  struct sockaddr_un *sockaddr=u8_alloc(struct sockaddr_un);
  sockaddr->sun_family=AF_LOCAL;
  strcpy(sockaddr->sun_path,string);
  return (struct sockaddr *)sockaddr;
#else
  return NULL;
#endif
}

static int sockaddr_samep(struct sockaddr *s1,struct sockaddr *s2)
{
  if (s1->sa_family != s2->sa_family) return 0;
  else if (s1->sa_family==AF_INET) {
    struct sockaddr_in *a1=(struct sockaddr_in *)s1;
    struct sockaddr_in *a2=(struct sockaddr_in *)s2;
    if (a1->sin_port!=a2->sin_port) return 0;
    else return ((memcmp(&(a1->sin_addr),&(a2->sin_addr),4))==0);}
#if HAVE_SYS_UN_H
  else if (s1->sa_family==AF_UNIX)
    return ((strcmp(((struct sockaddr_un *)s1)->sun_path,
		    ((struct sockaddr_un *)s2)->sun_path))==0);
#endif
#ifdef AF_INET6
  else if (s1->sa_family==AF_INET6) {
    struct sockaddr_in6 *a1=(struct sockaddr_in6 *)s1;
    struct sockaddr_in6 *a2=(struct sockaddr_in6 *)s2;
    if (a1->sin6_port!=a2->sin6_port) return 0;
    else return ((memcmp(&(a1->sin6_addr),&(a2->sin6_addr),8))==0);}
#endif
  else return 0;
}

/* Servers */

/* This provides a generic server loop which is customized by
   functions for accepting, servicing, and closing remote clients.
   The application conses client objects whose first few fields are
   reserved for used by the server. */

static void init_server_socket(u8_socket socket_id);

U8_EXPORT
/* u8_client_init:
    Arguments: a pointer to a client
    Returns: void
 Initializes the client structure.
 @arg client a pointer to a client stucture, or NULL if one is to be consed
 @arg len the length of the structure, as allocated or to be consed
 @arg sock a socket (or -1) for the client
 @arg server the server of which the client will be a part
 Returns: a pointer to a client structure, consed if not provided.
*/
u8_client u8_client_init(u8_client client,size_t len,
			 struct sockaddr *addrbuf,size_t addrlen,
			 u8_socket sock,u8_server srv)
{
  if (!(client)) client=u8_malloc(len);
  memset(client,0,len);
  client->socket=sock;
  client->server=srv;
  client->started=client->queued=client->active=-1;
  client->reading=client->writing=-1;
  client->off=client->len=client->buflen=client->delta=0;
  if ((srv->flags)&U8_SERVER_ASYNC)
    client->flags=client->flags|U8_CLIENT_ASYNC;
  return client;
}

U8_EXPORT
/* u8_client_done:
    Arguments: a pointer to a client
    Returns: void
 Marks the client's current task as done, updating server data structures
 appropriately. */
int u8_client_done(u8_client cl)
{
  U8_SERVER *server=cl->server;
  int clientid=cl->clientid;
  if (cl->started>0) {
    if (server->donefn) {
      int retval=server->donefn(cl);
      if (retval<0) {
	u8_log(LOG_ERR,"u8_client_done",
	       "Error when closing client @x%lx#%d/%d[%d](%s)",
	       ((unsigned long)cl),cl->clientid,cl->socket,cl->n_trans,cl->idstring);
	u8_clear_errors(1);}}
    update_client_stats(cl,u8_microtime(),1);
    cl->active=cl->writing=cl->reading=0;
    if (cl->queued>0) {
      u8_log(LOG_WARNING,"u8_client_done",
	     "Finishing transaction on a queued client @x%lx#%d/%d[%d](%s)",
	     ((unsigned long)cl),cl->clientid,cl->socket,cl->n_trans,cl->idstring);
      cl->queued=0;}

    if (cl->socket>0) {
      struct pollfd *pfd=&(server->sockets[clientid]);
      pfd->events=POLLIN|POLLHUP|POLLRDHUP;}
    u8_lock_mutex(&(server->lock));
    server->n_busy--; server->n_trans++;
    u8_unlock_mutex(&(server->lock));
    cl->started=0;
    return 1;}
  else {
    u8_log(LOG_WARNING,"u8_client_done",
	   "Declaring done on idle client @x%lx#%d/%d[%d](%s)",
	   ((unsigned long)cl),cl->clientid,cl->socket,cl->n_trans,cl->idstring);
    return 0;}
}

/* Closing clients */

U8_EXPORT
/* u8_client_close:
    Arguments: a pointer to a client
    Returns: int
 Marks the client's current task as done, updating server data structures
 appropriately, and ending by closing the client close function.
 If a busy client is closed, it has its U8_CLIENT_CLOSING flag set.
 This tells the event loop to finish closing the client when it actually
 completes the current transaction.
*/
int u8_client_close(u8_client cl)
{
  U8_SERVER *server=cl->server;
  if ((cl->flags&U8_CLIENT_CLOSED)==0) {
    int retval;
    u8_lock_mutex(&(server->lock));
    /* Catch race conditions */
    if (cl->flags&U8_CLIENT_CLOSED) {
      u8_unlock_mutex(&(server->lock));
      return 0;}
    cl->flags|=U8_CLIENT_CLOSING;
    u8_unlock_mutex(&(server->lock));
    if (cl->active>0) return 0;
    retval=client_close_core(cl,0);
    return retval;}
  else {
    u8_log(LOG_WARNING,"u8_client_close",
	   "Closing already closed socket @x%lx#%d/%d[%d](%s)",
	   ((unsigned long)cl),cl->clientid,cl->socket,cl->n_trans,cl->idstring);
    u8_unlock_mutex(&(server->lock));
    return 0;}
}

U8_EXPORT
/* u8_client_closed:
    Arguments: a pointer to a client
    Returns: int
  Indicates that the other end has closed the connection.  This can generate
    an indication of its own and then closes the client on this end
*/
int u8_client_closed(u8_client cl)
{
  if ((cl->server->flags)&(U8_SERVER_LOG_CONNECT))
    u8_log(LOG_NOTICE,"u8_client_closed",
	   "Other end closed @x%lx#%d/%d[%d](%s)",
	   ((unsigned long)cl),cl->clientid,cl->socket,
	   cl->n_trans,cl->idstring);
  return u8_client_close(cl);
}


U8_EXPORT
/* u8_client_shutdown:
    Arguments: a pointer to a client
    Returns: int
 Marks the client's current task as done, updating server data structures
 appropriately, and ending by closing the client close function.
 This will shutdown an active client.
*/
int u8_client_shutdown(u8_client cl)
{
  U8_SERVER *server=cl->server;
  if ((cl->flags&U8_CLIENT_CLOSED)==0) {
    int retval;
    u8_lock_mutex(&(server->lock));
    /* Catch race conditions */
    if (cl->flags&U8_CLIENT_CLOSED) {
      u8_unlock_mutex(&(server->lock));
      return 0;}
    cl->flags|=U8_CLIENT_CLOSING;
    u8_unlock_mutex(&(server->lock));
    retval=client_close_core(cl,0);
    return retval;}
  else {
    u8_log(LOG_WARNING,"u8_client_close",
	   "Closing already closed socket @x%lx#%d/%d[%d](%s)",
	   ((unsigned long)cl),cl->clientid,cl->socket,cl->n_trans,cl->idstring);
    u8_unlock_mutex(&(server->lock));
    return 0;}
}

/* This is the internal version used when shutting down a server.
   We dont wait for anything. */
static int client_close_for_shutdown(u8_client cl)
{
  int retval=0;
  if ((cl->flags&U8_CLIENT_CLOSED)==0) {
    if (cl->started>0) {
      /* It's in the middle of something, so we just flag it as closing. */
      cl->flags=cl->flags|(U8_CLIENT_CLOSING);
      return 0;}
    else return client_close_core(cl,1);}
  else return 0;
}

/* This is used when a transaction finishes and the socket has been
   closed during shutdown. */
static int client_close_core(u8_client cl,int server_locked)
{
  if (cl->flags&U8_CLIENT_CLOSED) return 0;
  else {
    U8_SERVER *server=cl->server;
    u8_socket sock=cl->socket;
    int clientid=cl->clientid, retval=0;
    /* We grab idstring, because our code allocated it but we will use it
       only after the closefn has freed the client object. */
    u8_string idstring=cl->idstring;
    long long cur=u8_microtime();
 
    /* Update run stats for one last time */
    if (cl->active>0) {
      u8_log(LOG_WARNING,"client_close_core",
	     "Closing active client @x%lx#%d/%d[%d](%s)",
	     (unsigned long long)cl,cl->clientid,cl->socket,
	     cl->n_trans,cl->idstring);
      update_client_stats(cl,cur,1);
      server->n_busy--;}
    if (cl->queued>0) {
      long long interval=cur-cl->started;
      u8_log(LOG_WARNING,"client_close_core",
	     "Closing a queued client @x%lx#%d/%d[%d](%s)",
	     ((unsigned long)cl),cl->clientid,cl->socket,
	     cl->n_trans,cl->idstring);
      cl->stats.qsum+=interval;
      cl->stats.qsum2+=(interval*interval);
      if (interval>cl->stats.qmax) cl->stats.qmax=interval;
      cl->stats.qcount++;}
    
    cl->queued=cl->active=cl->reading=cl->writing=cl->started=-1;

    if (sock>0) {
      struct pollfd *pfd=&(cl->server->sockets[clientid]);
      memset(pfd,0,sizeof(struct pollfd));
      pfd->fd=-1;}
    cl->flags|=U8_CLIENT_CLOSED;

    if (!(server_locked)) u8_lock_mutex(&(server->lock));
    server->n_clients--;
    update_server_stats(cl);
    server->clients[clientid]=NULL;
    /* If the newly empty slot is before the current free_slot,
       make it the free_slot. */
    if (clientid<server->free_slot) server->free_slot=clientid;
    if (!(server_locked)) u8_unlock_mutex(&(server->lock));

    if (server->closefn)
      retval=server->closefn(cl);
    else if (cl->socket>0) {
      retval=close(cl->socket); cl->socket=-1;}
    else retval=0;

    if (cl->server->flags&U8_SERVER_LOG_CONNECT)
      u8_log(LOG_NOTICE,ClosedClient,"Closed @x%lx#%d/%d[%d](%s)",
	     ((unsigned long)cl),cl->clientid,cl->socket,
	     cl->n_trans,cl->idstring);
    if ((cl->buf)&&(cl->ownsbuf)) u8_free(cl->buf);
    u8_free(idstring);
    u8_free(cl);

    return retval;}
}

static int finish_closing_client(u8_client cl)
{
  U8_SERVER *server=cl->server;
  if ((cl->flags&U8_CLIENT_CLOSED)==0) {
    int retval;
    u8_lock_mutex(&(server->lock));
    /* Catch race conditions */
    if (cl->flags&U8_CLIENT_CLOSED) {
      u8_unlock_mutex(&(server->lock));
      return 0;}
    u8_unlock_mutex(&(server->lock));
    retval=client_close_core(cl,0);
    u8_unlock_mutex(&(server->lock));
    return retval;}
  else return 0;
}

static void update_client_stats(u8_client cl,long long cur,int done)
{
  long long interval=cur-cl->active;
  /* active stats */
  cl->stats.asum+=interval;
  cl->stats.asum2+=(interval*interval);
  if (interval>cl->stats.amax) cl->stats.amax=interval;
  cl->stats.acount++;
  /* read/write/execute */
  if (cl->writing) {
    interval=cur-cl->writing;
    cl->stats.wsum+=interval;
    cl->stats.wsum2+=(interval*interval);
    if (interval>cl->stats.wmax) cl->stats.wmax=interval;
    cl->stats.wcount++;}
  else if (cl->reading) {
    interval=cur-cl->reading;
    cl->stats.rsum+=interval;
    cl->stats.rsum2+=(interval*interval);
    if (interval>cl->stats.rmax) cl->stats.rmax=interval;
    cl->stats.rcount++;}
  else {
    interval=cur-cl->active;
    cl->stats.xsum+=interval;
    cl->stats.xsum2+=(interval*interval);
    if (interval>cl->stats.xmax) cl->stats.xmax=interval;
    cl->stats.xcount++;}
  if (done) {
    if (cl->started>0){
      interval=cur-cl->started;
      cl->stats.tsum+=interval;
      cl->stats.tsum2+=(interval*interval);
      if (interval>cl->stats.tmax) cl->stats.tmax=interval;
      cl->stats.tcount++;}
    else u8_log(LOG_WARNING,IdleClient,
		"Finishing off inactive client @x%lx#%d/%d[%d](%s)",
		((unsigned long)cl),cl->clientid,cl->socket,
		cl->n_trans,cl->idstring);}
}

static void update_server_stats(u8_client cl)
{
  u8_server server=cl->server;
  /* Transfer all the client statistics to the server */
  server->tsum+=cl->stats.tsum; server->tsum2+=cl->stats.tsum2; server->tcount+=cl->stats.tcount;
  if (cl->stats.tmax>server->tmax) server->tmax=cl->stats.tmax;
  server->asum+=cl->stats.asum; server->asum2+=cl->stats.asum2; server->acount+=cl->stats.acount;
  if (cl->stats.amax>server->amax) server->amax=cl->stats.amax;
  server->rsum+=cl->stats.rsum; server->rsum2+=cl->stats.rsum2; server->rcount+=cl->stats.rcount;
  if (cl->stats.rmax>server->rmax) server->rmax=cl->stats.rmax;
  server->wsum+=cl->stats.wsum; server->wsum2+=cl->stats.wsum2; server->wcount+=cl->stats.wcount;
  if (cl->stats.wmax>server->wmax) server->wmax=cl->stats.wmax;
  server->xsum+=cl->stats.xsum; server->xsum2+=cl->stats.xsum2; server->xcount+=cl->stats.xcount;
  if (cl->stats.xmax>server->xmax) server->xmax=cl->stats.xmax;
  server->n_errs+=cl->n_errs;
}

#if U8_THREADS_ENABLED
static u8_client pop_task(struct U8_SERVER *server)
{
  u8_client task=NULL;
  u8_lock_mutex(&(server->lock));
  while ((server->n_queued == 0) && ((server->flags&U8_SERVER_CLOSED)==0)) 
    u8_condvar_wait(&(server->empty),&(server->lock));
  if (server->flags&U8_SERVER_CLOSED) {}
  else if (server->n_queued) {
    task=server->queue[server->queue_head++];
    if (server->queue_head>=server->queue_len) server->queue_head=0;
    server->n_queued--;}
  else {}
  if ((task)&&((task->active>0))) {
    /* This should probably never happen */
    u8_log(LOG_CRIT,"pop_task(u8)","popping active task @x%lx#%d/%d[%d](%s)",
	   ((unsigned long)task),task->clientid,task->socket,
	   task->n_trans,task->idstring);
    task->queued=-1; task=NULL;}
  else if (task) {
    u8_utime curtime=u8_microtime();
    task->queued=-1; task->active=curtime;
    if (task->started<0) {
      task->started=curtime;
      server->n_busy++;}}
  else {}
  u8_unlock_mutex(&(server->lock));
  return task;
}

static int push_task(struct U8_SERVER *server,u8_client cl)
{
  if (server->n_queued >= server->max_queued) return 0;
  if (cl->queued>0) return 0;
  server->queue[server->queue_tail++]=cl;
  if (server->queue_tail>=server->queue_len) server->queue_tail=0;
  server->n_queued++;
  if (cl->active>0) {
    u8_utime cur=u8_microtime();
    long long atime=cur-cl->active;
    cl->stats.asum+=atime;
    cl->stats.asum2+=(atime*atime);
    if (atime>cl->stats.amax) cl->stats.amax=atime;
    cl->stats.acount++;
    cl->queued=cur;
    cl->active=-1;}
  else cl->queued=u8_microtime(); 
  server->sockets[cl->clientid].events=0;
  u8_condvar_signal(&(server->empty));
  return 1;
}

static void *event_loop(void *thread_arg)
{
  struct U8_SERVER *server=(struct U8_SERVER *)thread_arg;
  /* Check for additional thread init functions */
  while (1) {
    u8_client cl; int dobreak=0; int result=0, closed=0;
    u8_utime cur;
    /* Check that this thread's init functions are up to date */
    u8_threadcheck();
    cl=pop_task(server);
    if ((!(cl))||(cl->socket<0)) continue;
    if (((server->flags)&(U8_SERVER_LOG_TRANSACT))||
	((cl->flags)&(U8_CLIENT_LOG_TRANSACT)))
      u8_log(LOG_NOTICE,ClientRequest,
	     "Handling activity from @x%lx#%d/%d[%d](%s)",
	     ((unsigned long)cl),cl->clientid,cl->socket,
	     cl->n_trans,cl->idstring);
    if ((cl->reading>0)||(cl->writing>0)) {
      /* We're in the middle of reading or writing a chunk of data,
	 so we try a chunk. */
      ssize_t delta;
      if (cl->off<cl->len) { /* We're not done */
	if (cl->writing>0)
	  delta=write(cl->socket,cl->buf+cl->off,cl->len-cl->off);
	else delta=read(cl->socket,cl->buf+cl->off,cl->len-cl->off);
	if (((server->flags)&(U8_SERVER_LOG_TRANSFERS))||
	    ((cl->flags)&(U8_CLIENT_LOG_TRANSFERS)))
	  u8_log(LOG_NOTICE,((cl->writing>0)?("Writing"):("Reading")),
		 "Processed %d bytes for @x%lx#%d/%d[%d](%s)",delta,
		 ((unsigned long)cl),cl->clientid,cl->socket,
		 cl->n_trans,cl->idstring);
	if (delta>0) cl->off=cl->off+delta;}
      /* If we've still got data to read/write, we update the poll
	 structure to keep listening and continue in the event loop.  */
      if (cl->off<cl->len) {
	/* u8_lock_mutex(&server->lock); */
	if (cl->writing>0) 
	  server->sockets[cl->clientid].events=POLLOUT|POLLHUP|POLLRDHUP;
	else server->sockets[cl->clientid].events=POLLIN|POLLHUP|POLLRDHUP;
	/* u8_unlock_mutex(&server->lock);*/
	continue;}
      else {
	/* Otherwise, we're done with whatever reading or writing we
	   were doing, so we record stats. */
	u8_utime cur=u8_microtime(); long long xtime;
	if (cl->writing>0) {
	  long long wtime=cur-cl->writing;
	  cl->stats.wsum+=wtime;
	  cl->stats.wsum2+=(wtime*wtime);
	  if (wtime>cl->stats.wmax) cl->stats.wmax=wtime;
	  cl->stats.wcount++;}
	else {
	  long long rtime=cur-cl->reading;
	  cl->stats.rsum+=rtime;
	  cl->stats.rsum2+=(rtime*rtime);
	  if (rtime>cl->stats.rmax) cl->stats.rmax=rtime;
	  cl->stats.rcount++;}}}
    if (result>=0) {
      long long xtime;
      cur=u8_microtime();
      if (cl->callback) {
	void *state=cl->cbstate;
	u8_client_callback callback=cl->callback;
	cl->callback=NULL; cl->cbstate=NULL;
	result=callback(cl,state);}
      else result=server->servefn(cl);
      /* Record execution stats */
      xtime=u8_microtime()-cur;
      cl->stats.xsum+=xtime;
      cl->stats.xsum2+=(xtime*xtime);
      if (xtime>cl->stats.xmax) cl->stats.xmax=xtime;
      cl->stats.xcount++;}
    if (result<0) {
      u8_exception ex=u8_current_exception;
      u8_log(LOG_ERR,"event_loop",
	     "Error result from client @x%lx#%d/%d[%d](%s)",
	     ((unsigned long)cl),cl->clientid,cl->socket,
	     cl->n_trans,cl->idstring);
      if (cl->flags&U8_CLIENT_CLOSED) closed=1;
      else if (cl->flags&U8_CLIENT_CLOSING) {
	update_client_stats(cl,u8_microtime(),1);
	cl->active=0; finish_closing_client(cl); closed=1;
	if ((cl->buf)&&(cl->ownsbuf)) u8_free(cl->buf);
	cl->buf=NULL; cl->off=cl->len=cl->buflen=0; cl->ownsbuf=0;}
      else if (cl->active>0) u8_client_done(cl);
      while (ex) {
	u8_log(LOG_WARN,ClientRequest,
	       "Error during activity on @x%lx#%d/%d[%d](%s)",
	       ((unsigned long)cl),cl->clientid,cl->socket,
	       cl->n_trans,cl->idstring,u8_errstring(ex));
	u8_clear_errors(1);}
      cl->n_errs++;}
    else if (result==0) {
      u8_utime cur=u8_microtime();
      long long ttime=cur-cl->started;
      /* Request is completed */
      if (((server->flags)&U8_SERVER_LOG_TRANSACT)||
	  ((cl->flags)&U8_CLIENT_LOG_TRANSACT))
	u8_log(LOG_NOTICE,ClientRequest,
	       "Completed transaction with @x%lx#%d/%d[%d](%s)",
	       ((unsigned long)cl),cl->clientid,cl->socket,
	       cl->n_trans,cl->idstring);
      cl->n_trans++;
      if (server->flags&U8_SERVER_CLOSED) dobreak=1;
      if (cl->flags&U8_CLIENT_CLOSED) {
	u8_log(LOG_CRIT,Inconsistency,
	       "Result returned from closed client @x%lx#%d/%d[%d](%s)",
	       ((unsigned long)cl),cl->clientid,cl->socket,cl->n_trans,cl->idstring);
	closed=1; if ((cl->buf)&&(cl->ownsbuf)) u8_free(cl->buf);
	cl->buf=NULL; cl->off=cl->len=cl->buflen=0; cl->ownsbuf=0;}
      else if (cl->flags&U8_CLIENT_CLOSING) {
	cl->active=0; finish_closing_client(cl); closed=1;
	if ((cl->buf)&&(cl->ownsbuf)) u8_free(cl->buf);
	cl->buf=NULL; cl->off=cl->len=cl->buflen=0; cl->ownsbuf=0;}
      else {
	if (cl->active>0) u8_client_done(cl);
	if ((cl->buf)&&(cl->ownsbuf)) u8_free(cl->buf);
	cl->buf=NULL; cl->off=cl->len=cl->buflen=0; cl->ownsbuf=0;}}
    else if (((cl->reading>0)||(cl->writing>0))&&(cl->buf!=NULL)) {
      /* The execution function queued some data to read or write.
	 We give it an initial try and push the task (in some cases,
	 the operating system will just take it all and we can finish
	 up. */
      ssize_t delta;
      if (cl->writing>0)
	delta=write(cl->socket,cl->buf+cl->off,((size_t)(cl->len-cl->off)));
      else delta=read(cl->socket,cl->buf+cl->off,((size_t)(cl->len-cl->off)));
      if (((server->flags)&(U8_SERVER_LOG_TRANSFERS))||
	  ((cl->flags)&(U8_CLIENT_LOG_TRANSFERS)))
	u8_log(LOG_NOTICE,((cl->writing>0)?("Writing"):("Reading")),
	       "Processed %d bytes for @x%lx#%d/%d[%d](%s)",delta,
	       ((unsigned long)cl),cl->clientid,cl->socket,
	       cl->n_trans,cl->idstring);
      if (delta>0) cl->off=cl->off+delta;
      /* If we've still got data to read/write, we continue,
	 otherwise, we fall through */
      if (cl->off<cl->len) {
	/* Keep listening */
	if (cl->writing>0) 
	  server->sockets[cl->clientid].events=POLLOUT|POLLHUP|POLLRDHUP;
	else server->sockets[cl->clientid].events=POLLIN|POLLHUP|POLLRDHUP;}
      else push_task(server,cl);}
    else {
      /* Request is not yet completed, but we're not waiting
	 on input or output. */
      if (((server->flags)&U8_SERVER_LOG_TRANSACT)||
	  ((cl->flags)&U8_CLIENT_LOG_TRANSACT))
	u8_log(LOG_NOTICE,ClientRequest,
	       "Yield during request for @x%lx#%d/%d[%d](%s)",
	       ((unsigned long)cl),cl->clientid,cl->socket,
	       cl->n_trans,cl->idstring);}
    if (cl->active>0) {
      /* Record the stats on how much thread time has been spent
	 (how long the task has been active) */
      u8_utime cur=u8_microtime();
      long long atime=cur-cl->active;
      cl->stats.asum+=atime;
      cl->stats.asum2+=(atime*atime);
      if (atime>cl->stats.amax) cl->stats.amax=atime;
      cl->stats.acount++;
      cl->active=-1;
      if (dobreak) break;
      if (!(closed)) {
	/* Start listening again (it was stopped by pop_task() */
	if (cl->writing>0) 
	  server->sockets[cl->clientid].events=POLLOUT|POLLHUP|POLLRDHUP;
	else server->sockets[cl->clientid].events=POLLIN|POLLHUP|POLLRDHUP;}}}
  u8_threadexit();
  return NULL;
}
#endif

U8_EXPORT
struct U8_SERVER *u8_init_server
   (struct U8_SERVER *server,
    u8_client (*acceptfn)(u8_server,u8_socket,struct sockaddr *,size_t),
    int (*servefn)(u8_client),
    int (*donefn)(u8_client),
    int (*closefn)(u8_client),
    ...)
{
  int i=0;
  int flags=0, init_clients=DEFAULT_INIT_CLIENTS, n_threads=DEFAULT_NTHREADS;
  int maxback=MAX_BACKLOG, max_queue=DEFAULT_MAX_QUEUE;
  int  max_clients=DEFAULT_MAX_CLIENTS;
  va_list args; int prop; int retval;
  if (server==NULL) {
    server=u8_alloc(struct U8_SERVER);
    memset(server,0,sizeof(struct U8_SERVER));}
  va_start(args,closefn);
  while ((prop=va_arg(args,int))>0) {
    switch (prop) {
    case U8_SERVER_FLAGS:
      flags=flags|(va_arg(args,int)); continue;
    case U8_SERVER_NTHREADS:
      n_threads=(va_arg(args,int)); continue;
    case U8_SERVER_INIT_CLIENTS:
      init_clients=(va_arg(args,int)); continue;
    case U8_SERVER_MAX_QUEUE:
      max_queue=(va_arg(args,int)); continue;
    case U8_SERVER_MAX_CLIENTS:
      max_clients=(va_arg(args,int)); continue;
    case U8_SERVER_BACKLOG:
      max_clients=(va_arg(args,int)); continue;
    case U8_SERVER_LOGLEVEL: {
      int level=(va_arg(args,int));
      if (level>3) flags=flags|U8_SERVER_LOG_TRANSFERS;
      if (level>2) flags=flags|U8_SERVER_LOG_TRANSACT;
      if (level>1) flags=flags|U8_SERVER_LOG_CONNECT;
      if (level>0) flags=flags|U8_SERVER_LOG_LISTEN;
      continue;}
    default:
      u8_log(LOG_CRIT,"u8_server_init",
	     "Unknown property code %d for server",prop);
      continue;}}
  va_end(args);
  
  if (init_clients<=0) init_clients=1;
  server->serverid=NULL; server->flags=flags; server->shutdown=0;
  server->init_clients=init_clients; server->max_clients=max_clients;
  server->server_info=NULL; server->n_servers=0;
  server->clients=u8_alloc_n(init_clients,u8_client);
  memset(server->clients,0,sizeof(u8_client)*init_clients);
  server->n_clients=0; server->clients_len=init_clients;
  server->sockets=u8_alloc_n(init_clients,struct pollfd); 
  memset(server->sockets,0,sizeof(struct pollfd)*init_clients);
  server->free_slot=server->max_slot=0;
  server->max_backlog=((maxback<=0) ? (MAX_BACKLOG) : (maxback));
  server->acceptfn=acceptfn;
  server->servefn=servefn;
  server->closefn=closefn;
  server->donefn=donefn;
#if U8_THREADS_ENABLED
  u8_init_mutex(&(server->lock));
  u8_init_condvar(&(server->empty)); u8_init_condvar(&(server->full));  
  server->n_threads=n_threads;
  server->queue=u8_alloc_n(max_queue,u8_client);
  server->n_queued=0; server->max_queued=max_queue;
  server->queue_head=0; server->queue_tail=0; server->queue_len=max_queue;
  server->thread_pool=u8_alloc_n(n_threads,pthread_t);
  server->n_trans=0; /* Transaction count */
  server->n_accepted=0; /* Accept count (new clients) */
  i=0; while (i < n_threads) {
	 pthread_create(&(server->thread_pool[i]),
			pthread_attr_default,
			event_loop,(void *)server);
	 i++;}
#endif
  return server;
}

U8_EXPORT
int u8_server_init(struct U8_SERVER *server,
		   /* max_clients is currently ignored */
		   int maxback,int max_queue,int n_threads,
		   u8_client (*acceptfn)(u8_server,u8_socket,
					 struct sockaddr *,size_t),
		   int (*servefn)(u8_client),
		   int (*closefn)(u8_client))
{
  if (u8_init_server
      (server,acceptfn,servefn,NULL,closefn,
       U8_SERVER_NTHREADS,n_threads,
       U8_SERVER_INIT_CLIENTS,max_queue,
       U8_SERVER_MAX_QUEUE,max_queue,
       U8_SERVER_BACKLOG,maxback,
       -1))
    return 1;
  else return 0;
}

static int do_shutdown(struct U8_SERVER *server,int grace)
{
  int i=0, max_socket, n_servers=server->n_servers;
  int n_errs=0, idle_clients=0, active_clients=0, clients_len;
  u8_utime deadline=u8_microtime()+grace;
  struct pollfd *sockets; u8_client *clients;
  if (server->flags&U8_SERVER_CLOSED) return 0;
  u8_lock_mutex(&server->lock);
  if (server->flags&U8_SERVER_CLOSED) {
    u8_unlock_mutex(&server->lock);
    return 0;}
  else {
    sockets=server->sockets;
    clients=server->clients;
    clients_len=server->clients_len;}
  server->flags=server->flags|U8_SERVER_CLOSED;
  /* Close all the server sockets */
  u8_log(LOG_WARNING,ServerShutdown,"Closing %d listening socket(s)",n_servers);
  while (i<n_servers) {
    struct U8_SERVER_INFO *info=&(server->server_info[i++]);
    u8_socket socket=info->socket, poll_index=info->poll_index, retval=0;
    sockets[poll_index].fd=-1; sockets[poll_index].events=0;
    if (socket>=0) retval=close(socket);
    if (retval<0) {
      u8_log(LOG_WARNING,ServerShutdown,
	     "Error (%s) closing socket %d listening at %s",
	     strerror(errno),socket,info->idstring);
      n_errs++;}
    else if (server->flags&U8_SERVER_LOG_LISTEN)
      u8_log(LOG_NOTICE,ServerShutdown,"Closed socket %d listening at %s",
	     socket,info->idstring);
    if (info->idstring) u8_free(info->idstring);
    if (info->addr) u8_free(info->addr);
    info->idstring=NULL; info->addr=NULL; info->socket=-1;}
  if (n_errs)
    u8_log(LOG_WARNING,ServerShutdown,
	   "Closed %d listening socket(s) with %d errors",
	   n_servers,n_errs);
  else u8_log(LOG_NOTICE,ServerShutdown,"Closed %d listening socket(s)",
	      n_servers);
  if (server->server_info) {
    u8_free(server->server_info);
    server->server_info=NULL;}
  /* Close all the idle client sockets and mark all the busy clients closed */
  i=0; while (i<clients_len) {
    u8_client client=clients[i];    
    if (client) {
      if (client->started<0) {
	client_close_for_shutdown(client);
	clients[i]=NULL;
	sockets[i].fd=-1;
	sockets[i].events=0;
	idle_clients++;}
      else active_clients++;}
    i++;}
  u8_log(LOG_NOTICE,ServerShutdown,
	 "Closed %d idle client socket(s), %d active clients left (n_busy=%d)",
	 idle_clients,active_clients,server->n_busy);
#if U8_THREADS_ENABLED
  /* The busy clients will decrement server->n_busy when they're finished.
     We wait for this to happen, sleeping for one second intervals. */
  u8_condvar_broadcast(&server->empty);
  u8_unlock_mutex(&server->lock); sleep(1);
  if (server->n_busy) {
    /* Wait for the busy connections to finish or a timeout */
    u8_lock_mutex(&server->lock); 
    while ((server->n_busy)&&(u8_microtime()<deadline)) {
      u8_unlock_mutex(&server->lock);
      sleep(1);
      u8_lock_mutex(&server->lock);}}
  if (server->n_busy) {
    u8_log(LOG_CRIT,ServerShutdown,
	   "Forcing %d active socket(s) closed after %dus",
	   server->n_busy,grace);
    i=0; while (i<clients_len) {
      u8_client client=clients[i];    
      if (client) {
	client_close_for_shutdown(client);
	clients[i]=NULL;
	sockets[i].fd=-1;
	sockets[i].events=0;
	idle_clients++;}
      i++;}}
#endif
  u8_free(server->clients); server->clients=NULL;
  u8_free(server->sockets); server->sockets=NULL;
  server->clients_len=0;
#if U8_THREADS_ENABLED  
  u8_free(server->thread_pool); server->thread_pool=NULL;
  u8_free(server->queue); server->queue=NULL;
#endif
  u8_unlock_mutex(&server->lock);
  u8_destroy_mutex(&server->lock);
  u8_destroy_condvar(&(server->empty));
  u8_destroy_condvar(&(server->full));  
  return 1;
}

U8_EXPORT int u8_server_shutdown(struct U8_SERVER *server,int grace)
{
  if (grace)
    server->shutdown=grace;
  else server->shutdown=-1;
  return 0;
}

/* Opening various kinds of servers */

static void init_server_socket(u8_socket socket_id)
{
#if WIN32
  unsigned long nonblocking=1;
#endif
  /* We set the server socket to be non-blocking */
#if (defined(F_SETFL) && defined(O_NDELAY))
  fcntl(socket_id,F_SETFL,O_NDELAY);
#endif
#if (defined(F_SETFL) && defined(O_NONBLOCK))
  fcntl(socket_id,F_SETFL,O_NONBLOCK);
#endif
#if WIN32
  ioctlsocket(socket_id,FIONBIO,&nonblocking);
#else
  u8_log(LOG_WARNING,_("sockopt"),"Can't set server socket to non-blocking");
#endif
}

static u8_socket open_server_socket(struct sockaddr *sockaddr,int maxbacklog)
{
  u8_socket socket_id=-1, on=1, addrsize, family;
  if (sockaddr->sa_family==AF_INET) {
    family=AF_INET; addrsize=sizeof(struct sockaddr_in);}
#ifdef AF_INET6
  else if (sockaddr->sa_family==AF_INET6) {
    family=AF_INET6; addrsize=sizeof(struct sockaddr_in6);}
#endif
#if HAVE_SYS_UN_H
  else if (sockaddr->sa_family==AF_UNIX) {
    family=AF_UNIX; addrsize=sizeof(struct sockaddr_un);}
#endif
  else {
    u8_seterr(_("invalid sockaddr"),"open_server_socket",NULL);
    return -1;}
  socket_id=socket(family,SOCK_STREAM,0);
  if (socket_id < 0) {
    u8_graberr(-1,"open_server_socket:socket",u8_sockaddr_string(sockaddr));
    return -1;}
  else if (setsockopt(socket_id, SOL_SOCKET, SO_REUSEADDR,
		      (void *) &on, sizeof(on)) < 0) {
    u8_graberr(-1,"open_server_socket:setsockopt",u8_sockaddr_string(sockaddr));
    return -1;}
  if ((bind(socket_id,(struct sockaddr *) sockaddr,addrsize)) < 0) {
    u8_graberr(-1,"open_server_socket:bind",u8_sockaddr_string(sockaddr));
    return -1;}
  if ((listen(socket_id,maxbacklog)) < 0) {
    u8_graberr(-1,"open_server_socket:listen",u8_sockaddr_string(sockaddr));
    return -1;}
  else {
    init_server_socket(socket_id);
    return socket_id;}
}

static int add_socket(struct U8_SERVER *server,u8_socket sock,short events)
{
  u8_client *clients=server->clients;
  struct pollfd *sockets=server->sockets;
  int clients_len=server->clients_len;
  if (sock<0) return sock;
  else if (server->free_slot==server->clients_len) {
    /* Grow the arrays if neccessary */
    int cur_len=server->clients_len;
    int grow_by=server->init_clients;
    int new_len=cur_len+grow_by;
    clients=u8_realloc(clients,sizeof(u8_client)*new_len);
    sockets=u8_realloc(sockets,sizeof(struct pollfd)*new_len);
    if ((clients)&&(sockets)) {
      memset(clients+clients_len,0,sizeof(u8_client)*grow_by);
      memset(sockets+clients_len,0,sizeof(struct pollfd)*grow_by);
      server->clients=clients; server->sockets=sockets;
      server->clients_len=new_len;}
    else {
      u8_log(LOG_CRIT,"add_socket","Couldn't allocate more clients/sockets");
      return -1;}}
  if (server->free_slot==server->max_slot) {
    int slot=server->free_slot;
    struct pollfd *pfd=&(server->sockets[slot]);
    memset(pfd,0,sizeof(struct pollfd));
    pfd->fd=sock; pfd->events=events;
    server->free_slot++; server->max_slot++;
    return slot;}
  else {
    int slot=server->free_slot, max_slot=server->max_slot;
    struct pollfd *pfd=&(server->sockets[slot]);
    int i=slot+1; while (i<max_slot) {
      if ((!(clients[i]))&&(sockets[i].fd<0)) break;
      else i++;}
    server->free_slot=i;
    memset(pfd,0,sizeof(struct pollfd));
    pfd->fd=sock; pfd->events=events;
    return slot;}
}

static struct U8_SERVER_INFO *add_server
  (struct U8_SERVER *server,u8_socket sock,struct sockaddr *addr)
{
  u8_server_info info; int off=-1;
  if (server->server_info==NULL) {
    info=server->server_info=u8_alloc(struct U8_SERVER_INFO);
    server->n_servers++;}
  else {
    server->server_info=
      u8_realloc_n(server->server_info,server->n_servers+1,
		   struct U8_SERVER_INFO);
    info=&(server->server_info[server->n_servers]);
    memset(info,0,sizeof(struct U8_SERVER_INFO));
    server->n_servers++;}
  info->socket=sock; info->addr=addr;
  info->poll_index=add_socket(server,sock,POLLIN);
  return info;
}

static struct U8_SERVER_INFO *find_server
  (struct U8_SERVER *server,struct sockaddr *addr)
{
  if (server->server_info==NULL) return NULL;
  else {
    struct U8_SERVER_INFO *servers=server->server_info;
    int i=0;
    while (i<server->n_servers)
      if (sockaddr_samep(servers[i].addr,addr))
	return &(servers[i]);
      else i++;
    return NULL;}
}

static int add_server_from_spec(struct U8_SERVER *server,u8_string spec)
{
  if ((strchr(spec,'/')) ||
      ((strchr(spec,'@')==NULL) &&
       (strchr(spec,':')==NULL) &&
       (strchr(spec,'.'))))
    return u8_add_server(server,spec,-1);
  else if (((strchr(spec,'@'))==NULL)&&((strchr(spec,':'))==NULL)) {
    /* Spec is just a port */
    int portno=u8_get_portno(spec);
    return u8_add_server(server,NULL,u8_get_portno(spec));}
  else {
    int portno=-1;
    u8_byte _hostname[128], *hostname=u8_parse_addr(spec,&portno,_hostname,128);
    if ((!(hostname))||(portno<0))
      return u8_reterr(u8_BadPortSpec,"add_server_from_spec",u8s(spec));
    return u8_add_server(server,hostname,portno);}
}

U8_EXPORT
int u8_add_server(struct U8_SERVER *server,char *hostname,int port)
{
  if (hostname==NULL)
    if (port<=0) return -1;
    else {
      int n_servers=0;
      u8_string thishost=u8_gethostname();
      if (thishost) 
	n_servers=u8_add_server(server,thishost,port);
      else n_servers=-1;
      u8_free(thishost);
      return n_servers;}
  else if (port==0)
    return add_server_from_spec(server,hostname);
  else if (port<0) {
    /* Open a file socket */
    struct sockaddr *addr=get_sockaddr_file(hostname); 
    struct U8_SERVER_INFO *info;
    if (find_server(server,addr)) {
      u8_log(LOG_NOTICE,NewServer,"Already listening at %s",hostname);
      u8_free(addr); return 0;}
    else {
      u8_socket sock=open_server_socket(addr,server->max_backlog);
      if (sock<0) {
	u8_free(addr);
	return -1;}
      else info=add_server(server,sock,addr);
      info->idstring=u8_strdup(hostname);
      if (server->flags&U8_SERVER_LOG_LISTEN)
	u8_log(LOG_INFO,NewServer,"Listening at %s",hostname);
      return 1;}}
  else {
    int n_servers=0;
    struct hostent *hostinfo=u8_gethostbyname(hostname,AF_INET);
    if (hostinfo==NULL) return -1;
    else {
      unsigned char **addrs=(unsigned char **)
	((hostinfo) ? (hostinfo->h_addr_list) : (NULL));
      if (addrs==NULL) return 0;
      while (*addrs) {
	struct sockaddr *sockaddr=
	  get_sockaddr_in(AF_INET,port,*addrs,hostinfo->h_length);
	if (find_server(server,sockaddr)) {
	  u8_free(sockaddr); addrs++;}
	else {
	  struct U8_SERVER_INFO *info;
	  u8_socket sock=open_server_socket(sockaddr,server->max_backlog);
	  if (sock>=0) {
	    info=add_server(server,sock,sockaddr);
	    if (info) info->idstring=u8_sockaddr_string(sockaddr);
	    if (server->flags&U8_SERVER_LOG_LISTEN)
	      u8_log(LOG_NOTICE,NewServer,"Listening to %s",info->idstring);}
	  else {
	    u8_log(LOG_ERROR,u8_NetworkError,"Can't open one socket");
	    if (sockaddr) u8_free(sockaddr);
	    if (hostinfo) u8_free(hostinfo);
	    return -1;}
	  addrs++; n_servers++;}}
      if (hostinfo) u8_free(hostinfo);
      return n_servers;}}
}

/* The core server loop */

static int add_client(struct U8_SERVER *server,u8_client client)
{
  u8_socket sock=client->socket;
  int slot=add_socket(server,sock,POLLIN);
  if (slot<0) return slot;
  server->clients[slot]=client; client->clientid=slot;
  client->server=server; server->n_clients++;
  return slot;
}

static int server_accept(u8_server server,u8_socket i)
{
  /* If the activity is on the server socket, open a new socket */
  char addrbuf[1024];
  int addrlen=1024; u8_socket sock;
  memset(addrbuf,0,1024);
  sock=accept(i,(struct sockaddr *)addrbuf,&addrlen);
  if (sock < 0) {
    u8_log(LOG_ERR,u8_NetworkError,_("Failed accept on socket %d"),i);
    errno=0;
    return -1;}
  else {
    u8_client cl=server->acceptfn
      (server,sock,(struct sockaddr *)addrbuf,addrlen);
    if (cl) {
      u8_string idstring=
	((cl->idstring)?(cl->idstring):
	 (u8_sockaddr_string((struct sockaddr *)addrbuf)));
      server->n_accepted++;
      if (!(cl->idstring)) cl->idstring=idstring;
      add_client(server,cl);
      if (server->flags&U8_SERVER_LOG_CONNECT) 
	u8_log(LOG_NOTICE,NewClient,"Opened @x%lx#%d/%d[%d](%s)",
	       ((unsigned long)cl),cl->clientid,cl->socket,
	       cl->n_trans,cl->idstring);
      return 1;}
    else if (server->flags&U8_SERVER_LOG_CONNECT) {
      u8_string connid=
	u8_sockaddr_string((struct sockaddr *)addrbuf);
      u8_log(LOG_NOTICE,RejectedConnection,"Rejected connection from %s");
      u8_free(connid);
      return 0;}
    else return 0;}
}

static int socket_peek(u8_socket sock)
{
  unsigned char buf[5];
  int retval=recv(sock,buf,1,MSG_PEEK), oretval=0;
  return (retval>0);
}

/* This listens for connections and pushes tasks (unless we're not
   threaded, in which case it dispatches to the servefn right
   away).  */
static int server_listen(struct U8_SERVER *server)
{
  struct pollfd *sockets; u8_client *clients;
  int i, max_slot, n_actions, retval;
  /* Wait for activity on one of your open sockets */
  while ((retval=poll(server->sockets,server->max_slot,100)) == 0) {
    if (retval<0) return retval;
    if (server->shutdown) {
      do_shutdown(server,server->shutdown);
      return 0;}
    if (server->flags&U8_SERVER_CLOSED) return 0;}
  if (server->shutdown) {
    do_shutdown(server,server->shutdown);
    return 0;}
  /* Iterate over the range of sockets */
  u8_lock_mutex(&(server->lock));
  sockets=server->sockets; clients=server->clients;
  max_slot=server->max_slot;
  i=0; while (i <= max_slot) {
    u8_client client; short events;
    if (sockets[i].fd<0) {i++; continue;}
    else if (sockets[i].revents==0) {
      i++; continue;}
    else {
      client=clients[i];
      events=sockets[i].revents;}
    if ((client==NULL)&&(events&POLLIN)) 
      /* Server connection */
      retval=server_accept(server,sockets[i].fd);
    else if (client==NULL) {
      /* Error on server socket? */}
#if U8_THREADS_ENABLED
    else if (client->active>0) {
      /* A thread is working on this client.  Don't touch it. */}
    else if (events&(POLLHUP|POLLRDHUP)) {
      if ((client->server->flags)&(U8_SERVER_LOG_CONNECT))
	u8_log(LOG_NOTICE,"server_listen",
	       "Other end closed (HUP) @x%lx#%d/%d[%d](%s)",
	       ((unsigned long)client),client->clientid,
	       client->socket,client->n_trans,client->idstring);
      client_close_core(client,1);
      i++; continue;}
    else if (((events&POLLOUT)&&((client->writing)>0))||
	     ((events&POLLIN)&&((client->reading)>0))) {
      if (push_task(server,client)) {
	sockets[i].events=sockets[i].events&(~(POLLIN|POLLOUT));
	n_actions++;}}
    else if (events&POLLIN) {
      if (!(socket_peek(client->socket))) {
	/* No real data, so we close it (probably the other side closed)
	   the connection. */
	if ((client->server->flags)&(U8_SERVER_LOG_CONNECT))
	  u8_log(LOG_NOTICE,"server_listen",
		 "Other end closed (no data) @x%lx#%d/%d[%d](%s)",
		 ((unsigned long)client),client->clientid,
		 client->socket,client->n_trans,client->idstring);
	client_close_core(client,1);
	i++; continue;}
      else if (push_task(server,client)) n_actions++;
      else {}}
#else
    else if (events&POLLIN) {
      server->servefn(client);
      n_actions++;}
#endif
    else {}
    i++;}
  u8_unlock_mutex(&(server->lock));
  return n_actions;
}

U8_EXPORT
int u8_push_task(struct U8_SERVER *server,u8_client cl)
{
  int retval;
  u8_lock_mutex(&(server->lock));
  retval=push_task(server,cl);
  u8_unlock_mutex(&(server->lock));
  return retval;
}

U8_EXPORT
void u8_server_loop(struct U8_SERVER *server)
{
  while ((server->flags&U8_SERVER_CLOSED)==0) server_listen(server);
}

/* Getting server status */

U8_EXPORT u8_server_stats u8_server_statistics
  (u8_server server,struct U8_SERVER_STATS *stats)
{
  int i=0, lim;
  struct U8_CLIENT **clients;
  if (stats==NULL) stats=u8_alloc(struct U8_SERVER_STATS);
  memset(stats,0,sizeof(struct U8_SERVER_STATS));
  u8_lock_mutex(&(server->lock));
  stats->n_reqs=server->n_trans;
  stats->n_errs=server->n_errs;
  stats->n_complete=server->tcount;
  stats->tsum=server->tsum; stats->tsum2=server->tsum2; stats->tcount=server->tcount;
  stats->tmax=server->tmax;
  stats->asum=server->asum; stats->asum2=server->asum2; stats->acount=server->acount;
  stats->amax=server->amax;
  stats->rsum=server->rsum; stats->rsum2=server->rsum2; stats->rcount=server->rcount;
  stats->rmax=server->rmax;
  stats->wsum=server->wsum; stats->wsum2=server->wsum2; stats->wcount=server->wcount;
  stats->wmax=server->wmax;
  stats->xsum=server->xsum; stats->xsum2=server->xsum2; stats->xcount=server->xcount;
  stats->xmax=server->xmax;
  stats->n_errs=server->n_errs;
  clients=server->clients; lim=server->max_slot;
  while (i<lim) {
    u8_client cl=clients[i++];
    if (cl) {
      if (cl->started>0) stats->n_busy++;
      if (cl->active>0) stats->n_active++;
      if (cl->reading>0) stats->n_reading++;
      if (cl->writing>0) stats->n_writing++;      
      stats->tsum+=cl->stats.tsum; stats->tsum2+=cl->stats.tsum2;
      stats->tcount+=cl->stats.tcount;
      if (cl->stats.tmax>stats->tmax) stats->tmax=cl->stats.tmax;
      stats->asum+=cl->stats.asum; stats->asum2+=cl->stats.asum2;
      stats->acount+=cl->stats.acount;
      if (cl->stats.amax>stats->amax) stats->amax=cl->stats.amax;
      stats->rsum+=cl->stats.rsum; stats->rsum2+=cl->stats.rsum2;
      stats->rcount+=cl->stats.rcount;
      if (cl->stats.rmax>stats->rmax) stats->rmax=cl->stats.rmax;
      stats->wsum+=cl->stats.wsum; stats->wsum2+=cl->stats.wsum2;
      stats->wcount+=cl->stats.wcount;
      if (cl->stats.wmax>stats->wmax) stats->wmax=cl->stats.wmax;
      stats->xsum+=cl->stats.xsum; stats->xsum2+=cl->stats.xsum2;
      stats->xcount+=cl->stats.xcount;
      if (cl->stats.xmax>stats->xmax) stats->xmax=cl->stats.xmax;
      stats->n_errs+=cl->n_errs;}}
  u8_unlock_mutex(&(server->lock));
  return stats;
}

U8_EXPORT
u8_string u8_server_status(struct U8_SERVER *server,u8_byte *buf,int buflen)
{
  struct U8_OUTPUT out;
  if (buf) {U8_INIT_FIXED_OUTPUT(&out,buflen,buf);}
  else {U8_INIT_OUTPUT(&out,256);}
  u8_lock_mutex(&(server->lock));
  u8_printf
    (&out,
     "%s Config: %d/%d/%d threads/maxqueue/backlog; Clients: %d/%d/%d busy/active/ever; Requests: %d/%d/%d live/queued/total;\n",
     server->server_info->idstring,
     server->n_threads,server->max_queued,server->max_backlog,
     server->n_busy,server->n_clients,server->n_accepted,
     server->n_busy,server->n_queued,server->n_trans);
  u8_unlock_mutex(&(server->lock));
  return out.u8_outbuf;
}

U8_EXPORT
u8_string u8_server_status_raw(struct U8_SERVER *server,u8_byte *buf,int buflen)
{
  struct U8_OUTPUT out;
  if (buf) {U8_INIT_FIXED_OUTPUT(&out,buflen,buf);}
  else {U8_INIT_OUTPUT(&out,256);}
  u8_lock_mutex(&(server->lock));
  u8_printf
    (&out,"%s\t%d\%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
     server->server_info->idstring,
     server->n_threads,server->max_queued,server->max_backlog,
     server->n_busy,server->n_clients,server->n_accepted,
     server->n_busy,server->n_queued,server->n_trans);
  u8_unlock_mutex(&(server->lock));
  return out.u8_outbuf;
}

/* Initialize */

U8_EXPORT void u8_init_srvfns_c()
{
  u8_register_source_file(_FILEINFO);
}
