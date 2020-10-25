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
#include<unistd.h>
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
//##__VA_ARGS__ 宏前面加上##的作用在于，当可变参数的个数为0时，这里的##起到把前面多余的","去掉的作用,否则会编译出错
int restart = 0;
FILE* globalfp;
int
str_echo(int sockfd,FILE **writefp)
{
	ssize_t		n;
	char		buf[MAX_CMD_STR],length[10],totalbuf[MAX_CMD_STR+9]={0};
	int len,pin,recvpin,sendpin;
	char wfile[20]={0};
	char newname[20]={0};
	sprintf(wfile,"stu_srv_res_%d.txt",(int)getpid());
again:
	while ( (n = read(sockfd, &pin, sizeof(pin))) > 0){
		if(n==0)return -1;
		recvpin=ntohl(pin);
		sendpin=htonl(recvpin);
		//printf("[%d]clipin recv %16x and send %16x\n",sockfd,recvpin,sendpin);
		n = read(sockfd, &len, sizeof(len));
		n = read(sockfd, buf, ntohl(len));
		bprintf(*writefp,"[echo_rqt](%d) %s\n",recvpin,buf);
		fflush(stdout);
		len = strnlen(buf,MAX_CMD_STR)+1;
		buf[len-1]='\0';
		
		*(int*)(&(totalbuf[0]))=sendpin;
		*(int*)(&(totalbuf[4]))=htonl(len);
		sprintf(&totalbuf[8],"%s",buf);
		write(sockfd, totalbuf, len+8);
		// write(sockfd, &sendpin, sizeof(sendpin));
		// write(sockfd, &len, sizeof(len));
		// write(sockfd, buf, len);
//		printf("buf %s\n",buf);
		memset(buf,0,sizeof(buf));
		memset(totalbuf,0,sizeof(totalbuf));
	}	
	if (n < 0 && errno == EINTR)
		goto again;
	else if (n < 0)
		printf("str_echo: read error\n");

	
	bprintf(*writefp,"[srv](%d) res file rename done!\n",(int)getpid());	
	bprintf(*writefp,"[srv](%d) connfd is closed!\n",(int)getpid());
	bprintf(*writefp,"[srv](%d) child process is going to exit!\n",(int)getpid());
	printf("[srv](%d) %s is closed!\n",(int)getpid(),wfile);//只打印到stdout
	fclose(*writefp);
	sprintf(newname,"stu_srv_res_%d.txt",recvpin);
	rename(wfile,newname);
	
	return recvpin;
}

void sig_int(int signo){
	bprintf(globalfp,"[srv] SIGINT is coming!\n");
	fflush(stdout);
}

void sig_pip(int signo){
	printf("[srv] SIGPIPE is coming!\n");
	fflush(stdout);
}
void sig_chld(int signo){
    wait(0);
    pid_t pid_chld;
	while((pid_chld=waitpid(-1,NULL,WNOHANG)>0));
	printf("[srv](%d) server child(%d) terminated.\n",(int)getpid(),pid_chld);
	restart=1;
}

int
main(int argc, char **argv)
{
	int					listenfd, connfd ,childpid;
	pid_t				childpin;
	socklen_t			clilen;
	struct sockaddr_in	cliaddr, servaddr;
	sigset_t sigset;
	struct sigaction act1,act2,act3;
	errno=0;
	if (argc != 3){
        printf("usage: tcpcli <IPaddress>");
        exit(0);
    }

	char fname[20]={0};
	
	if((globalfp=fopen("stu_srv_res_p.txt","wr"))==NULL){
		printf("file error");
	}
	printf("[srv](%d) stu_srv_res_p.txt is opened!\n",(int)getpid());

	// sigemptyset(&act1.sa_mask);
	// act1.sa_handler=sig_pip;
    // act1.sa_flags=0;
	// sigaction(SIGPIPE,&act1,NULL);
	
	sigemptyset(&act2.sa_mask);
	act2.sa_handler=sig_int;
    act2.sa_flags=0;
    sigaction(SIGINT,&act2,NULL);

	sigemptyset(&act3.sa_mask);
	act2.sa_handler=sig_chld;
    act2.sa_flags=SA_RESTART;
    sigaction(SIGCHLD,&act2,NULL);

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
	servaddr.sin_port = htons(atoi(argv[2]));//host to network short
	//servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	//servaddr.sin_port        = htons(9877);
	bprintf(globalfp,"[srv](%d) server[%s:%d] is initializing!\n",(int)getpid(),argv[1],atoi(argv[2]));
	fflush(stdout);
	bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

	listen(listenfd, 1024);

	for (int i=1 ;i<100 ;i++ ) {//to protect
		char saveipv4[20];		
		clilen = sizeof(cliaddr);
		connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);
		inet_ntop(AF_INET,(void*)&cliaddr.sin_addr,saveipv4,20);
		//printf("%d %d",connfd,errno);
		// if(restart==1){
		// 	restart=0;
		// 	continue;
		// }
		if(connfd==-1&&errno==EINTR){
			printf("connfd %d errno %d\n",connfd,errno);
			break;
		}	
		bprintf(globalfp,"[srv](%d) client[%s:%d] is accepted!\n",(int)getpid(),saveipv4,ntohs(cliaddr.sin_port));	
		if ( (childpid = fork()) == 0) {	/* child process */
			close(listenfd);	/* close listening socket */
			char fname[20]={0};
			FILE* fp;
			sprintf(fname,"stu_srv_res_%d.txt",(int)getpid());
			if((fp=fopen(fname,"wr"))==NULL){
				printf("file error");
			}
			printf("[srv](%d) stu_srv_res_%d.txt is opened!\n",(int)getpid(),(int)getpid());	
			bprintf(fp,"[srv](%d) child process is created!\n",(int)getpid());
			bprintf(fp,"[srv](%d) listenfd is closed!\n",(int)getpid());
			str_echo(connfd,&fp);	/* process the request */
			close(connfd);
			exit(0);
		}
		close(connfd);/* parent closes connected socket */					
	}
	close(listenfd);
	bprintf(globalfp,"[srv] listenfd is closed!\n");
	bprintf(globalfp,"[srv](%d) parent process is going to exit!\n",(int)getpid());
	fclose(globalfp);
	printf("[srv](%d) stu_srv_res_p.txt is closed!\n",(int)getpid());//只打印到stdout

	exit(0);
}
