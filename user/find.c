#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void to_file_name(char* path,char* file_name){
    int length = strlen(path);
    int name_begin=0;
    for(int i=length-1;i>=0;i--){
        if(path[i]=='/'){
            name_begin=i+1;
            break;
        }
    }
    memcpy(file_name,path+name_begin,length-name_begin+1);
    return;
}

void find_file(char* path,char* target_name){
    int fd;
    struct stat st;
    char* p;
    char buf[512];
    struct dirent de;
    char filename[128];
    int buf_length;

    //把fd对应的文件的信息写入到st当中去
    if(stat(path, &st) < 0){ 
        fprintf(2, "find: cannot stat %s\n", path);
        return;
    }
  
    switch(st.type){
    // 若是设备,匹配
    case T_DEVICE:
    // 若是文件,匹配
    case T_FILE:
        
        to_file_name(path,filename);

        if(strcmp(target_name,filename)==0)
            printf("%s\n",path);
        return;

    // 若是文件夹，找其下的每一个，递归
    case T_DIR:
        //打开文件夹的写指针
        if((fd = open(path, 0)) < 0){ //在linux里面，文件夹的路径也代表一个文件，也可以用open打开
                                      //"Directory is a file containing a sequence of dirent structures."
            fprintf(2, "find: cannot open %s\n", path);
        return;
        }

        if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
            printf("find: path too long\n");
            break;
        }

        // 在path后面加上/
        strcpy(buf, path);  //path->buf
        buf_length = strlen(buf);
        p = buf+buf_length;  //把p移到最后，p是一个char*
        *p++ = '/';  //*(p++) 优先级一致，右结合

        while(read(fd, &de, sizeof(de)) == sizeof(de)){
            if(de.inum == 0)
                continue;
            if((strcmp(".",de.name)==0)||(strcmp("..",de.name)==0)){
                continue;
            }
            // 把文件名字加进来，递归
            p = buf+buf_length+1;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;

            find_file(buf,target_name);
        }
    return;

    }
    return;
}

// int main(int argc,char* argv[]){
int main(int argc,char* argv[]){

    if(argc<2){
        printf("Need a path and a filename!\n");
        exit(0);
    }

    find_file(argv[1],argv[2]);

    exit(0);
}