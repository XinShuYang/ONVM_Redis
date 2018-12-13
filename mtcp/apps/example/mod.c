#define _GNU_SOURCE
#define __USE_GNU
#define MAX_FILES 30
#define NAME_LIMIT 256
#define FULLNAME_LIMIT 512
#include "mod.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h> 
#include <assert.h>
#include <sys/socket.h>
#include <dirent.h>

#include <mtcp_api.h>

#include "debug.h" 

struct file_cache
{
    char name[NAME_LIMIT];
    char fullname[FULLNAME_LIMIT];
    uint64_t size;
    char *file;
};

static char *conf_file = "epserver.conf";
static int core_limit;
const char *www_main = "www";
static int nfiles;
static struct file_cache fcache[MAX_FILES];
static int finished;

void setconf1(){
    struct mtcp_conf mcfg;
    int ret;
    int fd;
    core_limit=1;
    DIR *dir;
    uint64_t total_read;
    struct dirent *ent;
    //conf_file = "epserver.conf";
    
    dir = NULL;
    dir = opendir(www_main);
    
    mtcp_getconf(&mcfg);
    mcfg.num_cores = core_limit;
    mtcp_setconf(&mcfg);
    printf("%s*******",conf_file);
    
    nfiles = 0;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0)
            continue;
        else if (strcmp(ent->d_name, "..") == 0)
            continue;
        
        snprintf(fcache[nfiles].name, NAME_LIMIT, "%s", ent->d_name);
        snprintf(fcache[nfiles].fullname, FULLNAME_LIMIT, "%s/%s",
                 www_main, ent->d_name);
        fd = open(fcache[nfiles].fullname, O_RDONLY);
        if (fd < 0) {
            perror("open");
            continue;
        } else {
            fcache[nfiles].size = lseek64(fd, 0, SEEK_END);
            lseek64(fd, 0, SEEK_SET);
        }
        
        fcache[nfiles].file = (char *)malloc(fcache[nfiles].size);
        if (!fcache[nfiles].file) {
            TRACE_CONFIG("Failed to allocate memory for file %s\n",
                         fcache[nfiles].name);
            perror("malloc");
            continue;
        }
        
        TRACE_INFO("Reading %s (%lu bytes)\n",
                   fcache[nfiles].name, fcache[nfiles].size);
        total_read = 0;
        while (1) {
            ret = read(fd, fcache[nfiles].file + total_read,
                       fcache[nfiles].size - total_read);
            if (ret < 0) {
                break;
            } else if (ret == 0) {
                break;
            }
            total_read += ret;
        }
        if (total_read < fcache[nfiles].size) {
            free(fcache[nfiles].file);
            continue;
        }
        close(fd);
        nfiles++;
        
        if (nfiles >= MAX_FILES)
            break;
    }
    
    finished = 0;
    
    ret = mtcp_init(conf_file);
    if(ret){
        printf("init fail!");
        return;
    }
    mtcp_getconf(&mcfg);
    mtcp_core_affinitize(0);
}



int main(){ setconf1();return 0;}

