//
//  newsimple.c
//  Server
//
//  Created by xsy on 12/3/18.
//  Copyright © 2018 xsy. All rights reserved.
//
#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

#include <mtcp_api.h>
#include <mtcp_epoll.h>
//#include <sys/epoll.h>

#include "debug.h"
#include "cpu.h"
/*----------------------------------------------------------------------------*/
#include <stdint.h>
#define USEC_PER_SEC 1000000
#define READ_CHUNK 16384
/*----------------------------------------------------------------------------*/
#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)
#define MAX_FLOW_NUM  (10000)

#define RCVBUF_SIZE (2*1024)
#define SNDBUF_SIZE (8*1024)

#define MAX_EVENTS (MAX_FLOW_NUM * 3)

#define HTTP_HEADER_LEN 1024
#define URL_LEN 128

#ifndef MAX_CPUS
#define MAX_CPUS        16
#endif
#define MAX_FILES 30

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef ERROR
#define ERROR (-1)
#endif

#define HT_SUPPORT FALSE

int xsy = 1;

/*----------------------------------------------------------------------------*/
struct thread_context
{
    mctx_t mctx;
    int ep;
};
/*----------------------------------------------------------------------------*/
static int num_cores;
static int core_limit;
static int value_size;
static pthread_t app_thread[MAX_CPUS];
static int done[MAX_CPUS];

/*----------------------------------------------------------------------------*/
void
CloseConnection(struct thread_context *ctx, int sockid)
{
    mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_DEL, sockid, NULL);
    mtcp_close(ctx->mctx, sockid);
}

/*----------------------------------------------------------------------------*/
int
AcceptConnection(struct thread_context *ctx, int listener)
{
    //printf("test3:%ld\n", gettid());
    mctx_t mctx = ctx->mctx;
    struct mtcp_epoll_event ev;
    int c;
    
    c = mtcp_accept(mctx, listener, NULL, NULL);
    
    if (c >= 0) {
        if (c >= MAX_FLOW_NUM) {
            TRACE_ERROR("Invalid socket id %d.\n", c);
            return -1;
        }
        TRACE_APP("New connection %d accepted.\n", c);
        
        //printf("New connection %d accepted.\n", c);
        ev.events = MTCP_EPOLLIN;
        ev.data.sockid = c;
        mtcp_setsock_nonblock(ctx->mctx, c);
        mtcp_epoll_ctl(mctx, ctx->ep, MTCP_EPOLL_CTL_ADD, c, &ev);
        TRACE_APP("Socket %d registered.\n", c);
        
    } else {
        if (errno != EAGAIN) {
            TRACE_ERROR("mtcp_accept() error %s\n",
                        strerror(errno));
        }
    }
    
    return c;
}
/*----------------------------------------------------------------------------*/
struct thread_context *
InitializeServerThread(int core)
{
    struct thread_context *ctx;
    
    /* affinitize application thread to a CPU core */
#if HT_SUPPORT
    mtcp_core_affinitize(core + (num_cores / 2));
#else
    mtcp_core_affinitize(core);
#endif /* HT_SUPPORT */
    
    ctx = (struct thread_context *)calloc(1, sizeof(struct thread_context));
    if (!ctx) {
        TRACE_ERROR("Failed to create thread context!\n");
        return NULL;
    }
    
    /* create mtcp context: this will spawn an mtcp thread */
    ctx->mctx = mtcp_create_context(core);
    if (!ctx->mctx) {
        TRACE_ERROR("Failed to create mtcp context!\n");
        return NULL;
    }
    
    /* create epoll descriptor */
    ctx->ep = mtcp_epoll_create(ctx->mctx, MAX_EVENTS);
    if (ctx->ep < 0) {
        TRACE_ERROR("Failed to create epoll descriptor!\n");
        return NULL;
    }
    
    return ctx;
}
/*----------------------------------------------------------------------------*/
int
CreateListeningSocket(struct thread_context *ctx)
{
    int listener;
    struct mtcp_epoll_event ev;
    struct sockaddr_in saddr;
    int ret;
    
    /* create socket and set it as nonblocking */
    listener = mtcp_socket(ctx->mctx, AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        TRACE_ERROR("Failed to create listening socket!\n");
        return -1;
    }
    ret = mtcp_setsock_nonblock(ctx->mctx, listener);
    if (ret < 0) {
        TRACE_ERROR("Failed to set socket in nonblocking mode.\n");
        return -1;
    }
    
    /* bind to port 80 */
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(80);
    ret = mtcp_bind(ctx->mctx, listener,
                    (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        TRACE_ERROR("Failed to bind to the listening socket!\n");
        return -1;
    }
    
    /* listen (backlog: 4K) */
    ret = mtcp_listen(ctx->mctx, listener, 4096);
    if (ret < 0) {
        TRACE_ERROR("mtcp_listen() failed!\n");
        return -1;
    }
    
    /* wait for incoming accept events */
    ev.events = MTCP_EPOLLIN;
    ev.data.sockid = listener;
    mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_ADD, listener, &ev);
    
    return listener;
}
/*----------------------------------------------------------------------------*/
void *
RunServerThread(void *arg)
{
    //    printf("Runserver %d\n",gettid());
    int core = *(int *)arg;
    struct thread_context *ctx;
    mctx_t mctx;
    int listener;
    int ep;
    struct mtcp_epoll_event *events;
    int nevents;
    int i, ret;
    int do_accept;
    
    //int len;
    char send[value_size+20];
    char val[value_size+10];
    //char send[100];
    int buffer_idx;
    char buffer[1024];
    
    printf("value_size=%d\n",value_size);
    //sprintf(send, "VALUE key 0 %d\r\n\0", value_size);
    for(i = 0; i< value_size; i++)
        val[i] = 'f';
    val[i] = '\0';
    strcat(send, val);
    strcat(send, "\r\nEND\r\n");
    
    //sprintf(send, "VALUE key 0 5\r\naaaaaaaaaa\r\nEND\r\n");
    /* initialization */
    ctx = InitializeServerThread(core);
    if (!ctx) {
        TRACE_ERROR("Failed to initialize server thread.\n");
        exit(-1);
    }
    mctx = ctx->mctx;
    ep = ctx->ep;
    
    events = (struct mtcp_epoll_event *)
    calloc(MAX_EVENTS, sizeof(struct mtcp_epoll_event));
    if (!events) {
        TRACE_ERROR("Failed to create event struct!\n");
        exit(-1);
    }
    
    listener = CreateListeningSocket(ctx);
    if (listener < 0) {
        TRACE_ERROR("Failed to create listening socket.\n");
        exit(-1);
    }
    
    while (!done[core]) {
        nevents = mtcp_epoll_wait(mctx, ep, events, MAX_EVENTS, -1);
        if (nevents < 1) {
            if (errno != EINTR)
                perror("mtcp_epoll_wait");
            break;
        }
        
        do_accept = FALSE;
        for (i = 0; i < nevents; i++) {
            if (events[i].data.sockid == listener) {
                /* if the event is for the listener, accept connection */
                do_accept = TRUE;
            } else if (events[i].events) {
                ret = mtcp_read(mctx, events[i].data.sockid, buffer, sizeof(1024));
                if (ret <= 0) {
                    if (ret == EAGAIN) printf("read() returned EAGAIN");
                    continue;
                }
                buffer_idx = ret;
                buffer[buffer_idx] = '\0';
                char *start = buffer;
                
                // Locate a \r\n
                char *crlf = NULL;
                printf("\n%s\n",buffer);
                while (start < &buffer[buffer_idx]) {
                    crlf = strstr(start, "\r\n");
                    if (crlf == NULL) break; // No \r\n found.
                    int length = crlf - start;
                    events[i].events = MTCP_EPOLLIN | MTCP_EPOLLOUT;
                    mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_MOD, events[i].data.sockid, &events[i]);
                    if (mtcp_write(mctx, events[i].data.sockid, send, strlen(send)) == EAGAIN)
                        printf("writev() returned EAGAIN\n");
                    start += length + 2;
                }
            } else {
                assert(0);
            }
        }
        
        /* if do_accept flag is set, accept connections */
        if (do_accept) {
            while (1) {
                ret = AcceptConnection(ctx, listener);
                if (ret < 0)
                    break;
            }
        }
    }
    
    /* destroy mtcp context: this will kill the mtcp thread */
    mtcp_destroy_context(mctx);
    pthread_exit(NULL);
    
    return NULL;
}
/*----------------------------------------------------------------------------*/
void
SignalHandler(int signum)
{
    int i;
    
    for (i = 0; i < core_limit; i++) {
        if (app_thread[i] == pthread_self()) {
            //TRACE_INFO("Server thread %d got SIGINT\n", i);
            done[i] = TRUE;
        } else {
            if (!done[i]) {
                pthread_kill(app_thread[i], signum);
            }
        }
    }
}
/*----------------------------------------------------------------------------*/

int
main(int argc, char **argv)
{
    //int fd;
    int ret;
    
    int cores[MAX_CPUS];
    int i;
    
    num_cores = GetNumCPUs();
    core_limit = num_cores;
    
    if (argc < 2) {
        TRACE_ERROR("$%s enter thread number\n", argv[0]);
        return FALSE;
    }
    
    for (i = 0; i < argc - 1; i++) {
        if (strcmp(argv[i], "-N") == 0) {
            core_limit = atoi(argv[i + 1]);
            if (core_limit > num_cores) {
                TRACE_CONFIG("CPU limit should be smaller than the "
                             "number of CPUS: %d\n", num_cores);
                return FALSE;
            }
        }
        if (strcmp(argv[i], "-V") == 0) {
            value_size = atoi(argv[i + 1]);
            //printf("Value_size=%d\n", value_size);
        }
        else
            value_size = 64;
    }
    /* initialize mtcp */
    ret = mtcp_init("epserver.conf");
    if (ret) {
        TRACE_ERROR("Failed to initialize mtcp\n");
        exit(EXIT_FAILURE);
    }
    /* register signal handler to mtcp */
    mtcp_register_signal(SIGINT, SignalHandler);
    
    TRACE_INFO("Application initialization finished.\n");
    
    
    for (i = 0; i < core_limit; i++) {
        cores[i] = i;
        done[i] = FALSE;
        
        if (pthread_create(&app_thread[i],
                           NULL, RunServerThread, (void *)&cores[i])) {
            perror("pthread_create");
            TRACE_ERROR("Failed to create server thread.\n");
            exit(-1);
        } 
    }
    
    //RunServerThread((void *)&cores[0]);
    
    
    for (i = 0; i < core_limit; i++) {
        pthread_join(app_thread[i], NULL);
    }
    
    mtcp_destroy();
    return 0;
}

