// 一定要fork??
#include "../kernel/types.h"
#include "./user.h"
#include "kernel/param.h"

int more_to_read(int argc,char* prime[],char* params[]){

}


int main(int argc,char* argv[]){

    if(argc==1){
        printf("Xargs need at least one params!");
    }

    char*  params[MAXARG];

    while(more_to_read(argc,argv,params)){
        if(fork()==0){
            exec(argv[0],params);
            exit(0);
        }
        else{
            wait(0);
        }
    }
    exit(0);
    
}