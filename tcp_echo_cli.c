#include<sys/socket.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<string.h>
#include<unistd.h>
#include<signal.h>
#define MAX_CMD_STR 200
#define bprintf(fp,format,...)\
	do{\
		if (fp==NULL)printf(format,##__VA_ARGS__);\
		else{\
			printf(format,##__VA_ARGS__);\
			fprintf(fp,format,##__VA_ARGS__);\
			fflush(fp);\
		}\
	}while (0);

void
str_cli(int pin, int sockfd,FILE **writefp)
{
	char	sendline[MAX_CMD_STR+1],recvline[MAX_CMD_STR+1],length[10],totalbuf[MAX_CMD_STR+9]={0};
	ssize_t		n;		
	FILE *readfp;
	char rfile[10]={0},wfile[20]={0};
	int clipintosend=htonl(pin),clipintorecv;
	sprintf(rfile,"td%d.txt",pin);
	sprintf(wfile,"stu_cli_res_%d.txt",pin);
	if((readfp=fopen(rfile,"r"))==NULL){
		printf("read file error\n");
	}

	while (fgets(sendline,MAX_CMD_STR-9,readfp)!=NULL)
	{	
		if(strncmp(sendline,"exit",4)==0)goto echoend;
		int len = strnlen(sendline,MAX_CMD_STR);
		sendline[len-1]='\0';
		
		*(int*)(&(totalbuf[0]))=clipintosend;
		*(int*)(&(totalbuf[4]))=htonl(len);
		sprintf(&totalbuf[8],"%s",sendline);
		write(sockfd, totalbuf, len+8);

		n = read(sockfd, &clipintorecv, sizeof(clipintorecv));	
		n = read(sockfd, &len, sizeof(len));
		n = read(sockfd, recvline, ntohl(len));
		if(clipintosend!=clipintorecv){
			printf("pin error %d %d %d %d\n\n",pin,clipintosend,clipintorecv,len);
			exit(0);
		}

		bprintf(*writefp,"[echo_rep](%d) %s\n",(int)getpid(),recvline);
		fflush(stdout);
		memset(recvline,0,sizeof(recvline));
		memset(sendline,0,sizeof(sendline));
		memset(totalbuf,0,sizeof(totalbuf));
	}
echoend:	
	close(sockfd);
	bprintf(*writefp,"[cli](%d) connfd is closed!\n",(int)getpid());
	if(pin==0){//大坑，宏定义不一定代表一行语句
		bprintf(*writefp,"[cli](%d) parent process is going to exit!\n",(int)getpid());
	}
	else{
		bprintf(*writefp,"[cli](%d) children process is going to exit!\n",(int)getpid());
	} 
	fclose(readfp);
	fclose(*writefp);
	printf("[cli](%d) %s is closed!\n",(int)getpid(),wfile);
}

void sig_int(int signo){
	printf("[srv] SIGINT is coming!\n");//其实是不合适的做法，有导致死锁的风险
	fflush(stdout);
	exit(0);
}
void sig_pip(int signo){
	printf("[srv] SIGPIPE is coming!\n");
	fflush(stdout);
}
void sig_chd(int signo){
    wait(0);
    return;
}

int
main(int argc, char **argv)
{
	int	sockfd,multis,pid;
	struct sockaddr_in	servaddr;
	struct sigaction act1,act2,act3;
	if (argc != 4){
        printf("usage: tcpcli <IPaddress>");
        exit(0);
    }
	multis=atoi(argv[3]);
	
	sigemptyset(&act1.sa_mask);
	act1.sa_handler=sig_pip;
    act1.sa_flags=SA_RESTART;//确保受到信号触发的慢调用函数重启而非返回异常
	sigaction(SIGPIPE,&act1,NULL);

	sigemptyset(&act2.sa_mask);
	act2.sa_handler=sig_int;
    act2.sa_flags=SA_RESTART;
    sigaction(SIGINT,&act2,NULL);

	sigemptyset(&act3.sa_mask);
	act2.sa_handler=sig_chd;
    act2.sa_flags=SA_RESTART;
    sigaction(SIGCHLD,&act2,NULL);

	for(int pin=multis-1;pin>=0;pin--){//son process start from 1
		if(pin==0)
		{
			sockfd = socket(AF_INET, SOCK_STREAM, 0);
			char fname[20]={0};
			FILE* fp;
			sprintf(fname,"stu_cli_res_%d.txt",pin);
			if((fp=fopen(fname,"wr"))==NULL){
				printf("file error");
			}
			printf("[cli](%d) stu_cli_res_%d.txt is created!\n",(int)getpid(),pin);
			bprintf(fp,"[cli](%d) parent process %d is created!\n",(int)getpid(),pin);
			fflush(stdout);

			memset(&servaddr,0,sizeof(servaddr));
			servaddr.sin_family = AF_INET;
			servaddr.sin_port = htons(atoi(argv[2]));//host to network short
			inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
			connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));//强转细节，因为第二个参数要求传入的是sockaddr
			bprintf(fp,"[cli](%d) server[%s:%d] is connected!\n",(int)getpid(),argv[1],atoi(argv[2]));
			fflush(stdout);

			str_cli(pin, sockfd,&fp);		/* do it all */	
			continue;
		}else if ((pid = fork())==0)
		{
			sockfd = socket(AF_INET, SOCK_STREAM, 0);
			char fname[20]={0};
			FILE* fp;
			sprintf(fname,"stu_cli_res_%d.txt",pin);
			if((fp=fopen(fname,"wr"))==NULL){
				printf("file error");
			}
			printf("[cli](%d) stu_cli_res_%d.txt is created!\n",(int)getpid(),pin);
			bprintf(fp,"[cli](%d) child process %d is created!\n",(int)getpid(),pin);
			fflush(stdout);

			memset(&servaddr,0,sizeof(servaddr));
			servaddr.sin_family = AF_INET;
			servaddr.sin_port = htons(atoi(argv[2]));//host to network short
			inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
			connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
			bprintf(fp,"[cli](%d) server[%s:%d] is connected!\n",(int)getpid(),argv[1],atoi(argv[2]));
			fflush(stdout);

			str_cli(pin, sockfd,&fp);		/* do it all */
			exit(0);
		}
	}	
}
