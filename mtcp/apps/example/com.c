#include <stdio.h>
#include <sys/socket.h>
#include "mod.h"
#include <mtcp_api.h>
int key;

int main(int argc, const char * argv[]) {
    // insert code here...
    key=1;
    mod();
    setconfm();
   // key=socket(1, 2, 3);
    printf("Hello, World!%d\n",key);
   // printt();
    return 0;
    
}
