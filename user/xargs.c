// 一定要fork??
#include "../kernel/types.h"
#include "./user.h"
#include "kernel/param.h"

#define MAX_LENGTH 30

int more_to_read(int argc,char* argv[],char* params[]){
    // params=(char(*)[MAX_LENGTH])params;
    //先把初始参数拷贝进去,注意第一个是args,不要 
    for(int i=1;i<argc;i++){
        strcpy(params[i-1],argv[i]);
    }

    //再从参数开始读。读到空格，就下一个字符串；读到\n，就下一个命令了
    char a;
    int param_cnt = argc-1;
    int letter_cnt = 0;
    while(read(0,&a,sizeof(char))){
        // printf("a is %c\n",a);
        if(a==' '){
            *(params[param_cnt]+letter_cnt)=0;
            param_cnt++;
            letter_cnt=0;
        }
        else if(a=='\n'){
            params[param_cnt][letter_cnt]='\0';
            param_cnt++;
            params[param_cnt]=0;

            return 1;
        }
        else{
            *(params[param_cnt]+letter_cnt)=a;
            letter_cnt++;
        }
    }



    return 0;
}


int main(int argc,char* argv[]){

    if(argc==1){
        printf("Xargs needs at least one params!");
    }

    // char params[MAXARG][MAX_LENGTH]={};
    // printf("init %p,force transfer %p, transfer0 %p\n",params[0],*(((char**)params)+0),((char**)params)[0]);
    
    // params is an array of MAXARG pointers to char
    // must use a malloc to allocate memory
    char **params;
    params = (char**)malloc(MAXARG * sizeof(char*));
    for (int i = 0; i < MAXARG; i++)
        params[i] = (char*)malloc((MAX_LENGTH+1) * sizeof(char));


    // 为什么一定要fork——因为exec直接自己结束了，除非出错
    while(more_to_read(argc,argv,params)){
        if(fork()==0){
            exec(argv[1],params);//除非出错，永不返回
            exit(0);
        }
        else{
            wait(0);
        }
    }
    free(params);
    exit(0);
    
}