#include"./user.h"
#include"../kernel/types.h"

#define N 40

void find_prime(int read_end,int read_num){
    if(read_num==0){
        exit(0);
    }
    int num[N];
    int p[2];
    int remain=0;
    int smallest_prime;
    pipe(p);
    read(read_end,num,read_num*sizeof(int));
    close(read_end);
    smallest_prime=num[0];
    printf("prime %d",smallest_prime);
    for(int i=1;i<read_num;i++){
        if((num[i]%smallest_prime)!=0){
            remain++;
            write(p[1],num+i,sizeof(int));
        }
    }
    close(p[1]);
    if(fork()==0){
        find_prime(p[0],remain);
    }
    else{
        wait(0);
        exit(0);
    }
}

int main(int argc,char* argv[]){
    int p[2];
    int all=34;


    pipe(p);
    if(fork()==0){
        close(p[1]);
        find_prime(p[0],all);
    }
    else{
        close(p[0]);
        for (int i=2;i<=35;i++){
            //管道的反复写入是可以的，并且最后也是按顺序的，但是要注意，
            //后面一个参数的意思是字节数，那么需要读的是num*sizeof(int)
            write(p[1],&i,sizeof(int));
        }
        close(p[1]);
        wait(0);
    }

    exit(0);

}

// int main(int argc,char* argv[]){
//     int p[2];
//     int cnt=34;
//     int num[40];
//     int i;
//     int temp=0;
//     int smallest_prime;
//     for(i=2;i<=35;i++){
//         num[i-2]=i;
//     }
//     while(cnt!=0){
//         pipe(p);
//         if(fork()!=0){
//             close(p[0]);
//             write(p[1],num,sizeof(int)*cnt);
//             close(p[1]);
//         }
//         else{
//             close(p[1]);
//             read(p[0],buffer,sizeof(int)*cnt);
//             smallest_prime = num[0];
//             temp=0;
//             printf("prime %d",num[0]);
//             for(i=0;i<cnt;i++){
//                 if((num[i]%smallest_prime)!=0){
//                     num[temp++]=num[i];
//                 }
//             }

//         }
//     }


// }