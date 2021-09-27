
#include "../kernel/types.h"
#include "./user.h"


int main(){



int ping[2];
int pong[2];

pipe(ping);
pipe(pong);

char* son_send="pong";
char son_receive[10];
char* par_send="ping";
char par_receive[10];

if (fork()==0){
    close(ping[1]);
    read(ping[0],son_receive,strlen(par_send));
    printf("%d: received %s\n",getpid(),son_receive);
    close(ping[0]);
    close(pong[0]);
    write(pong[1],son_send,strlen(son_send));
    close(pong[1]);
}
else{
    close(ping[0]);
    write(ping[1],par_send,strlen(par_send));
    close(ping[1]);
    close(pong[1]);
    read(pong[0],par_receive,strlen(son_send));
    close(pong[0]);
    printf("%d: received %s\n",getpid(),par_receive);
}

exit(0);

}