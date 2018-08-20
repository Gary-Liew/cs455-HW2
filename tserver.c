#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
//The <pwd.h> header shall provide a definition for struct passwd
#include <pwd.h>
//The <stddef.h> header defines the following:NULL,Null pointer constant.
#include <stddef.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <ctype.h>
#include <openssl/md5.h>
//允许连接server的最大数量的clienT.
#define LISTENQ 1024
//buf里存的最大的char的个数
#define MAXLINE 2048

#define FILETYPE_REQ 0xea
#define FILETYPE_RSP 0xe9
#define FILETYPE_ERR 0xe8

#define CHECKSUM_REQ 0xca
#define CHECKSUM_RSP 0xc9
#define CHECKSUM_ERR 0xc8

#define DOWNLOAD_REQ 0xaa
#define DOWNLOAD_RSP 0xa9
#define DOWNLOAD_ERR 0xa8

#define UNKNOWN_FAIL 0x51

struct msg_head{
	char type;
    //uint32_t is a numeric type that guarantees 32 bits, the value is unsigned, meaning that the range of values goes from 0 to 2^32 - 1
	uint32_t length;
} STRUCT_PACKED;

int listenfd,connfd;
int flag=0;
int timeout=300;
FILE *file=NULL;
unsigned char outmd[16];

int Socket(int family,int type,int protocol)
{
	int n;
	if((n=socket(family,type,protocol))<0)
		fprintf(stderr,"socket error");
	return n;
}

void Bind(int fd,const struct sockaddr *sa,socklen_t slen)
{
	int n;
	if((n=bind(fd,sa,slen))<0)
	{
		fprintf(stderr,"bind error\n");
		exit(1);
	}
}

void Listen(int fd,int backlog)
{
	if(listen(fd,backlog)<0)
		fprintf(stderr,"listen error");
}

int Accept(int fd,struct sockaddr *sa,socklen_t *slen)
{
	int n;
	if((n=accept(fd,sa,slen))<0)
	{
		fprintf(stderr,"accept error");
	}
	return n;
}

ssize_t Writen(int fd, const void *vptr, size_t n)  
{  
    size_t nleft;
    ssize_t nwritten;
    const char *ptr;
  
    ptr = vptr;
    nleft = n;
    while(nleft > 0)
    {
        //ssize_t write (int fd, const void * buf, size_t count),write()会把参数buf所指的内存写入count个字节到参数放到所指的文件内
    	if((nwritten = write(fd, ptr, nleft)) <= 0)
    	{
    		if (nwritten < 0 && errno == EINTR)
            	nwritten = 0;

            return -1; 
        }

        nleft -= nwritten;
        ptr   += nwritten;
    }

    return n;
}

ssize_t Readn(int fd, void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nread;
    char *ptr;

    ptr = vptr;
    nleft = n;
    while(nleft > 0) 
    {
        //read函数fd所指的文件传送nbyte个字节到buf指针所指的内存中。若参数nbyte为0，则read()不会有作用并返回0。返回值为实际读取到的字节数，如果返回0，表示已到达文件尾或无可读取的数据。错误返回-1,并将根据不同的错误原因适当的设置错误码。
        if((nread = read(fd, ptr, nleft)) < 0)
        {
            //永远阻塞的系统调用是指调用有可能永远无法返回，多数网络支持函数都属于这一类。EINTR错误的产生：当阻塞于某个慢系统调用的一个进程捕获某个信号且相应信号处理函数返回时，该系统调用可能返回一个EINTR错误。例如：在socket服务器端，设置了信号捕获机制，有子进程，当在父进程阻塞于慢系统调用时由父进程捕获到了一个有效信号时，内核会致使accept返回一个EINTR错误(被中断的系统调用)。
        	if (errno == EINTR)
        		nread = 0;
        	else
        		return -1;
        }
        else if (nread == 0)
        	break;

        nleft -= nread;
        ptr += nread;
    }

    return n - nleft;
}


void Close(int fd)
{
	if(close(fd)==-1)
		fprintf(stderr,"close error");
}

void Popen(const char* command,const char* type)
{
	char tmp[2048],*ptr;
	ptr = tmp;
    //int snprintf(char *restrict buf, size_t n, const char * restrict  format, ...);
    //函数说明:最多从源串中拷贝n－1个字符到目标串中，然后再在后面加一个0。所以如果目标串的大小为n 的话，将不会溢出。
	sprintf(tmp,"file ");
	ptr += 5;
	sprintf(ptr,"%s",command);

	if((file = popen(tmp,type))==NULL)
	{
		fprintf(stderr,"popen error");
	}
}

int md5_sum(const char *filename,uint32_t offset,int32_t length)
{
	int len=0,n,s;
	MD5_CTX ctx;
	char buf[MAXLINE];
	FILE* fp=NULL;
	struct stat stat_buf;	
	uint32_t fsize;
	int size;

	stat(filename,&stat_buf);
    //st_size – total file size, in bytes
	fsize = stat_buf.st_size;

	if((fp=fopen(filename,"r"))==NULL)
	{
		return 0;
	}	
	
	if((length<0) && (offset<fsize))
	{
		size = fsize-offset+1;
	}
	else if((length>0) && (fsize>=(offset+length)))
	{
		size = length;
	}
	else if((length>0) && (fsize<(offset+length)))
	{
		return 0;
	}
    //int fseek(FILE *stream, long offset, int fromwhere),函数设置文件指针stream的位置
    //如果执行成功，stream将指向以fromwhere为基准，偏移offset（指针偏移量）个字节的位置，函数返回0。
	if((n=fseek(fp,offset,SEEK_SET))==-1)
	{
		return 0;
	}
	MD5_Init(&ctx);

	s = size;
	while(s>MAXLINE)
	{
        //size_t fread(void *buffer, size_t size, size_t count, FILE *stream);
        //buffer 用于接收数据的内存地址,size 要读的每个数据项的字节数，单位是字节
        //count 要读count个数据项，每个数据项size个字节.
        //stream 输入流
        //返回真实读取的项数.
		len = fread(buf,1,MAXLINE,fp);
		MD5_Update(&ctx,buf,len);
		s = s-len;
	}
	len = fread(buf,1,s,fp);
	MD5_Update(&ctx,buf,len);
	MD5_Final(outmd,&ctx);
	
	fclose(fp);
	return 1;
}

void handle_sig()
{
	if(flag==1)
	{
        //The shutdown() call causes all or part of a full-duplex connection on
        //the socket associated with sockfd to be shut down.  If how is
        //SHUT_RD, further receptions will be disallowed.  If how is SHUT_WR,
        //further transmissions will be disallowed.  If how is SHUT_RDWR,
        //further receptions and transmissions will be disallowed.
		shutdown(listenfd,SHUT_WR);
	}
	printf("%d seconds timer has expired.Server has auto-shutdown.\n",timeout);
	close(listenfd);
	exit(1);
}
static void usage(void)
{
	fprintf(stderr,
			"usage: tserver [-d] [-t seconds] port"
			"\n");
	exit(1);
}

void normal_replace(char line[], char from, char to)
{
	char *p=line;
	while(*p) 
	{
		if (*p==from)
			*p=to;
		p++;
	}
}

int check_filename(char *pos)
{
	int i;
	for(i=0;i<strlen(pos);i++)
	{
		if( islower(pos[i]) || isupper(pos[i]) || isdigit(pos[i]) || 
		    pos[i]=='-' || pos[i]=='_' || pos[i]=='+' || pos[i]=='.' ||
		    pos[i]==',' || pos[i]=='/')
		{
			continue;
		}
		else 
		{
			return -1;
		}
	}
	return 0;
}

int main(int argc,char** argv)
{
	int port;
	int debug;
	int isSend;
	char recvline[MAXLINE],recvmsg[MAXLINE],line[MAXLINE];;
	char buf[MAXLINE],*sptr, *pos;
	int md_flag;
	int i=0;
    //size_t is a type able to represent the size of any object in bytes: size_t is the type returned by the
    //sizeof operator and is widely used in the standard library to represent sizes and counts.
	size_t fsize;		
	uint32_t offset;
    //int32_t stores 32 bits integer.
	int32_t len, n, s, t;
    //socklen_t, which is an unsigned opaque integral type of length of at least 32 bits.
	socklen_t clilen;
	struct sockaddr_in cliaddr,servaddr;
   	
	for(;;)
	{
		int c;
		c=getopt(argc,argv,"dht:");
		if(c<0)
			break;
		switch(c)
		{
			case 'h':
				usage();
				break;
			case 't':
				timeout=atoi(optarg);
				if(timeout < 5)
				{
					fprintf(stderr,"timeout must >= 5\n");
					exit(1);
				}
				break;
			case 'd':
				debug=1;
				break;
			default:
				usage();
				break;
		}
	}
	if(optind==argc)
	{
		usage();
	}
	const char *filename;	
	port = atoi(argv[optind]);
	
	int opt=1,ret=1;
	listenfd=Socket(AF_INET,SOCK_STREAM,0);
    //int setsockopt(int socket, int level, int option_name,const void *option_value, socklen_t option_len);
    //the setsockopt() function shall set the option specified by the option_name argument, at the protocol
    //level specified by the level argument, to the value pointed to by the option_value argument for the socket
    //associated with the file descriptor specified by the socket argument.
    //closesocket（一般不会立即关闭而经历TIME_WAIT的过程）后想继续重用该socket：
	ret=setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    //置字节字符串s的前n个字节为零且包括‘\0’
	bzero(&servaddr,sizeof(servaddr));

	servaddr.sin_family=AF_INET;
    //INADDR_ANY转换过来就是0.0.0.0，泛指本机的意思，也就是表示本机的所有IP，因为有些机子不止一块网卡，多网卡的情况下，这个就表示所
    //有网卡ip地址的意思。
	servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);  
	
	struct msg_head* server_Rsp;
	server_Rsp = (struct msg_head*) malloc(sizeof(struct msg_head));

	int reuse = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) ;
	Bind(listenfd,(struct sockaddr*) &servaddr,sizeof(servaddr));
	//Linux提供了alarm系统调用和SIGALRM信号实现使用定时器功能。为避免进程陷入无限期的等待，能够为这些堵塞式系统调用设置定时器。
    //在定时器设置的超时时间到达后，调用alarm的进程将收到SIGALRM信号。
    //sigset() Catch SIGALRM
	sigset(SIGALRM,handle_sig);
	alarm(timeout);

	Listen(listenfd,LISTENQ);
	
	while(1)
	{	
		flag=1;
		connfd = Accept(listenfd,(struct sockaddr*)NULL,NULL);

		int n = Readn(connfd,recvline,MAXLINE);
		struct msg_head *recvdata = (struct msg_head *)recvline;
		pos = recvline + sizeof(struct msg_head);
		
		int data_len = ntohl(recvdata->length);		
		switch((unsigned char)recvdata->type)
		{
			case FILETYPE_REQ:

				if(debug==1)
				{
					printf("FILETYPE_REQ received with DataLength=%d, Data=%s\n",
									ntohl(recvdata->length), pos);
				}
				
				if(i=check_filename(pos)==-1)
				{	
					server_Rsp->type = FILETYPE_ERR;
					server_Rsp->length = htonl(0);
                    //memcpy函数的功能是从源src所指的内存地址的起始位置开始拷贝n个字节到目标dest所指的内存地址的起始位置中。void *memcpy(void *dest, const void *src, size_t n);
					memcpy(buf,server_Rsp,sizeof(struct msg_head));
					Writen(connfd,buf,sizeof(struct msg_head));
					break;
				}
				
				Popen(pos,"r");
                //char *fgets(char *str, int n, FILE *stream)，It reads a line from the specified stream and stores it into the string pointed to by str.
				fgets(line,MAXLINE,file);
				normal_replace(line, '\t', ' ');
                //void *memset(void *s, int ch, size_t n);
                //函数解释：将s中当前位置后面的n个字节用ch替换并返回s 。
				memset(buf,0,MAXLINE);
				server_Rsp->type = FILETYPE_RSP;	
				server_Rsp->length = htonl(strlen(line));
				memcpy(buf,server_Rsp,sizeof(struct msg_head));
				Writen(connfd,buf,sizeof(struct msg_head));

				memcpy(buf,line,strlen(line));
				Writen(connfd,buf,strlen(line));

				if(debug==1)
				{
					printf("FILETYPE_RSP sent with DataLength=%d, Data=%s\n",
							strlen(line),line);
				}	
				pclose(file);
				break;

			case CHECKSUM_REQ:

				memcpy(&offset, pos, sizeof(uint32_t));

				pos += sizeof(uint32_t);			
				memcpy(&len, pos, sizeof(int32_t));
				
				pos += sizeof(int32_t);
				filename = pos;
				if(debug==1)
				{
					printf("CHECKSUM_REQ received with Datalength=%d, offset = %PRIu32, length=%PRIu32,filename=%s\n",data_len,offset,len,pos);
				}
				
				md_flag = md5_sum(filename,offset,len);
				if(md_flag == 1)	
				{
					memset(buf,0,MAXLINE);
					server_Rsp->type = CHECKSUM_RSP;
					server_Rsp->length = htonl(16);
					memcpy(buf,server_Rsp,sizeof(struct msg_head));
					Writen(connfd,buf,sizeof(struct msg_head));

					memcpy(buf,outmd,16);
					Writen(connfd,buf,16);

					if(debug==1)
					{
						printf("CHECKSUM_RSP sent with DataLength=%d,checksum=",16);
						for(i=0;i<16;i++)
						{
							printf("%02x",outmd[i]);
						}
						printf("\n");
					}
				}
				else
				{
					server_Rsp->type = CHECKSUM_ERR;
					server_Rsp->length = htonl(0);
					memcpy(buf,server_Rsp,sizeof(struct msg_head));
					Writen(connfd,buf,sizeof(struct msg_head));
					if(debug==1)
					{
						printf("CHECKSUM_ERR sent with DataLength=%d\n",0);
					}
				}

				break;

			case DOWNLOAD_REQ:

				memcpy(&offset, pos, sizeof(uint32_t));

				pos += sizeof(uint32_t);
				memcpy(&len, pos, sizeof(int32_t));

				pos += sizeof(int32_t);
				filename = pos;

				if(debug==1)
				{
					printf("DOWNLOAD_REQ received with DataLength=%d,offset=% PRIu32,length=%PRIu32,filename=%s\n",
							data_len,offset,len,pos);
				}

				md_flag = md5_sum(filename,offset,len);

				if((file = fopen(filename,"r"))==NULL)
				{
					server_Rsp->type = DOWNLOAD_ERR;
					server_Rsp->length = htonl(0);
					memcpy(buf,server_Rsp,sizeof(struct msg_head));
					Writen(connfd,buf,sizeof(struct msg_head));
					break;
				}
                //SEEK_END： 文件结尾
				fseek(file, 0, SEEK_END);
                //ftell函数用于得到文件位置指针当前位置相对于文件首的偏移字节数
				fsize = ftell(file);
				
				if((len<0) && (offset<fsize))
				{
					isSend = 1;
					len = fsize-offset;
				}
				else if((len>0) && (len+offset)<=fsize)
				{
					isSend = 1;
					len = len;
				}
				else 
				{
					isSend = 0;
				}
				if(isSend==1)
				{
					fseek(file, offset, SEEK_SET);
					memset(buf,0,MAXLINE);

					server_Rsp->type = DOWNLOAD_RSP;
                    //download数据信息包含data和md5sum两部分。所以长度为len+16.
					server_Rsp->length = htonl(len+16);
					memcpy(buf,server_Rsp,sizeof(struct msg_head));
					Writen(connfd,buf,sizeof(struct msg_head));

					if(debug==1)
					{
						printf("DOWNLOAD_RSP sent with DataLength=%d\n",len+16);
					}

					s = len;
					while(s>MAXLINE) 
					{
						n = fread(line,1,MAXLINE,file);
						Writen(connfd,line,n);
						s = s-n;
					}

					n = fread(line,1,s,file);
					Writen(connfd,line,n);
				}
				else 
				{
					if(debug==1)
					{
						printf("DOWNLOAD_ERR sent with DataLength=%d\n",0);
					}
					server_Rsp->type = DOWNLOAD_ERR;
					server_Rsp->length = htonl(0);
					memcpy(buf,server_Rsp,sizeof(struct msg_head));
					Writen(connfd,buf,sizeof(struct msg_head));
				}

				memcpy(buf,outmd,16);
				Writen(connfd,buf,16);

				fclose(file);
				break;

			default:

				server_Rsp->type = UNKNOWN_FAIL;
				server_Rsp->length = ntohl(4);
				memcpy(buf,server_Rsp,sizeof(struct msg_head));
				Writen(connfd,buf,sizeof(struct msg_head));

				if(debug==1)
				{
                    printf("Message with MessageType = 0x51 received,Ignored.\n");
					printf("UNKNOWN_FAIL sent with DataLength = %d\n",5);
				}				
		}
		Close(connfd);
	}
	Close(listenfd);
}
