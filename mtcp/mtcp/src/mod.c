#define _GNU_SOURCE
#define __USE_GNU
#include "mod.h"
#include <dlfcn.h>
#include <assert.h>
#include <sys/socket.h>

#include <mtcp_api.h>

#include "debug.h"
#include <stdlib.h>

#define MAX_FLOW_NUM  (10000)
#define RCVBUF_SIZE (2*1024)
#define SNDBUF_SIZE (8*1024)

#define MAX_EVENTS (MAX_FLOW_NUM * 3)
#define HTTP_HEADER_LEN 1024
#define NAME_LIMIT 256

static int (*real_socket)(int,int,int);

static char *conf_file = "epserver.conf";
static int core_limit;

struct thread_context
{
    mctx_t mctx;
    int ep;
    struct server_vars *svars;
};

struct server_vars
{
    char request[HTTP_HEADER_LEN];
    int recv_len;
    int request_len;
    long int total_read, total_sent;
    uint8_t done;
    uint8_t rspheader_sent;
    uint8_t keep_alive;
    
    int fidx;                        // file cache index
    char fname[NAME_LIMIT];                // file name
    long int fsize;                    // file size
};

void setconfm(){
    struct mtcp_conf mcfg;
    int ret;
    core_limit=1;
    //conf_file = "epserver.conf";
    mtcp_getconf(&mcfg);
    mcfg.num_cores = core_limit;
    mtcp_setconf(&mcfg);
    
    int listener;
    struct sockaddr_in saddr;
    struct thread_context *ctx;
    
    printf("%s*******",conf_file);
    ret = mtcp_init(conf_file);
    if(ret){
        printf("init fail!");
        return;
    }
    mtcp_getconf(&mcfg);
    
    /*---------------------------*/

    mtcp_core_affinitize(0);
    ctx = (struct thread_context *)calloc(1, sizeof(struct thread_context));
    ctx->mctx = mtcp_create_context(0);
    ctx->ep = mtcp_epoll_create(ctx->mctx, MAX_EVENTS);
    if (ctx->ep < 0) {
        mtcp_destroy_context(ctx->mctx);
        free(ctx);
        TRACE_ERROR("Failed to create epoll descriptor!\n");
        return;
    }
    /* allocate memory for server variables */
    ctx->svars = (struct server_vars *)
    calloc(MAX_FLOW_NUM, sizeof(struct server_vars));
    if (!ctx->svars) {
        mtcp_close(ctx->mctx, ctx->ep);
        mtcp_destroy_context(ctx->mctx);
        free(ctx);
        TRACE_ERROR("Failed to create server_vars struct!\n");
        return;
    }
    
    listener = mtcp_socket(ctx->mctx, AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        TRACE_ERROR("Failed to create listening socket!\n");
        return ;
    }
    ret = mtcp_setsock_nonblock(ctx->mctx, listener);
    if (ret < 0) {
        TRACE_ERROR("Failed to set socket in nonblocking mode.\n");
        return ;
    }
    
    /* bind to port 80 */
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(80);
    ret = mtcp_bind(ctx->mctx, listener,
                    (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        TRACE_ERROR("Failed to bind to the listening socket!\n");
        return ;
    }
    
    /* listen (backlog: can be configured) */
    ret = mtcp_listen(ctx->mctx, listener, -1);
    printf("successful listen");
}
void mod(){
#define INIT_FUNCTION(func) \
real_##func = dlsym(RTLD_NEXT, #func); \
assert(real_##func);
    
    INIT_FUNCTION(socket);
    
#undef INIT_FUNCTION
    
    
    return;
}

int socket(int domain, int type, int protocol){
    
    
    return real_socket(domain,type,protocol);
}
//int main(){mod();return 0;}

