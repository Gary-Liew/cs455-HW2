#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <netdb.h>

#define SERV_PORT 12345
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
	uint32_t length;
} STRUCT_PACKED;

struct client_argument {
    char *host;
    int port;
    char *cmd_type;
    uint32_t offset;
    int32_t length;
    char *file_name;
    char *saveas_file_name;
} ac;

static void usage(void)
{
	fprintf(stderr,
			"usage: tclient [hostname:]port filetype filename"
			"\n"
			"tclient [hostname:]port checksum [-o offset] [-l length] filename"
			"\n"
			"tclient [hostaname:]port download [-o offert] [-l length] filename [saveasfilename]"
			"\n");
	exit(1);
}

void default_argument() 
{
    ac.host = "127.0.0.1";
    ac.port = 9876;
    ac.cmd_type = NULL;
    ac.offset = 0;
    ac.length = -1;
    ac.file_name = NULL;
    ac.saveas_file_name = NULL;
}

void parse_argument(int argc,char *argv[])
{
    int i=1;
    if(i<argc && i==1) 
    {
        //strrok切割字符串，将str切分成一个个子串
        //char* strtok (char* str,constchar* delimiters);
        //str：在第一次被调用的时间str是传入需要被切割字符串的首地址；在后面调用的时间传入NULL。
        //delimiters：表示切割字符串（字符串中每个字符都会 当作分割符）。
        char *ip=strtok(argv[i],":");
        char *port_str=strtok(NULL,":");
        if(port_str) 
		{
            ac.host=ip;
            ac.port=atoi(port_str);
        }
        else 
		{
            ac.host="127.0.0.1";
            ac.port=atoi(ip);
        }
    }
    else 
    {
    	printf("No address argument!\n");
    	usage();
    	return;
    }
    i++;
    if(i<argc && i==2)
    {
        ac.cmd_type=argv[i];
    }
    else 
    {
    	printf("No command type argument!\n");
    	usage();
    	return;
    }        
    if(strcmp(ac.cmd_type,"filetype") == 0) 
    {
        if(i+1 >= argc) 
		{
            printf("filetype must be followd by filename!\n");
            usage();
            return;
        }
        ac.file_name=argv[++i];
    }
    if(strcmp(ac.cmd_type,"checksum") == 0) 
    {           
        if(i+1 >= argc) 
		{
            printf("checksum must be followed by some arguments!\n");
            usage();
            return;
        }
        i++;
        while(i < argc) 
		{
            char *cmd=argv[i];
            if(strcmp(cmd,"-o") == 0) 
		    {
       	         if(i+1 >= argc) 
				 {
                    printf("-o must be followed by offset value!\n");
                    usage();
                    return;
                 }
                ac.offset=atoi(argv[++i]);
            } 
	    	else if(strcmp(cmd,"-l") == 0) 
	    	{
                if(i+1 >= argc) 
				{
                    printf("-l must be followed by length value!\n");
                    usage();
                    return;
                }
                ac.length=atoi(argv[++i]);
        	} 
	    	else 
	    	{
                ac.file_name=cmd;
                if(i+1 < argc) 
				{
                    printf("Wrong argument numbers!\n");
                    usage();
                    return;
                }
        	}
            i++;
        }
        if(ac.file_name == NULL) 
		{
            printf("No file name argument!\n");
            usage();
            return;
        }
    }
    if(strcmp(ac.cmd_type,"download") == 0) 
    {           
        if(i+1 >= argc) 
		{
            printf("download must be followed by some arguments!\n");
            usage();
            return;
        }
        i++;
        while(i<argc) 
		{
            char *cmd=argv[i];
            if(strcmp(cmd,"-o") == 0) 
	 	    {
                if(i+1 >= argc) 
				{
                    printf("-o must be followed by offset value!\n");
                    usage();
                    return;
                }
                ac.offset=atoi(argv[++i]);
            } 
	    	else if(strcmp(cmd,"-l") == 0) 
	    	{
                if(i+1 >= argc) 
				{
                    printf("-l must be followed by length value!\n");
                    usage();
                    return;
                }
                ac.length=atoi(argv[++i]);
        	} 
	    	else 
	    	{
                ac.file_name=cmd;
                if(i+1 < argc) 
				{
                    ac.saveas_file_name=argv[++i];
                }
                if(i+1 < argc) 
				{
                    printf("Wrong argument numbers!\n");
                    usage();
                    return;
                }
            }
            i++;
        }
        if(ac.file_name == NULL) 
		{
           	    printf("No file name argument!\n");
                usage();
            	return;
        }
    }
}

int Socket(int family,int type,int protocol)
{
	int n;
	if((n=socket(family,type,protocol))<0)
		perror("socket error");
	return n;
}

void Connect(int fd,const struct sockaddr *sa,socklen_t slen)
{
	if(connect(fd,sa,slen)<0)
		perror("connect error");
}

void Close(int fd)
{
	if(close(fd)==-1)
		perror("close error");
}

ssize_t Writen(int fd, const void *vptr, size_t n)  
{  
    size_t nleft;
    ssize_t nwritten;
    const char  *ptr;
  
    ptr = vptr;
    nleft = n;
    while(nleft > 0)
    {
        ////ssize_t write (int fd, const void * buf, size_t count),write()会把参数buf所指的内存写入count个字节到参数放到所指的文件内
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
    {   //read函数fd所指的文件传送nbyte个字节到buf指针所指的内存中。若参数nbyte为0，则read()不会有作用并返回0。返回值为实际读取到的字节数，如果返回0，表示已到达文件尾或无可读取的数据。错误返回-1,并将根据不同的错误原因适当的设置错误码
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

int main(int argc,char** argv)
{
	int sockfd,n,i;
	char recvline[MAXLINE], data[MAXLINE];
	char buf[MAXLINE],saveasfile[512],*pos;
	size_t fsize;
	FILE *file;
	size_t psize;
	int s,t;

	const char* filetype = "filetype";
	const char* checksum = "checksum";
	const char* download = "download";

	char *host;
	int port;
	uint32_t offset;
	int32_t len;
	char *type;
	char *filename;
	char *savefilename;
	char userinput[16],*savefilepath;
	size_t data_len;

	default_argument();
	parse_argument(argc, argv);

	host = ac.host;
	port = ac.port;
	offset = ac.offset;
	len = ac.length;
	type = ac.cmd_type;
	filename = ac.file_name;
	savefilepath = ac.saveas_file_name;

	struct msg_head* client_Req;
	client_Req = (struct msg_head*) malloc(sizeof(struct msg_head));

	if(strcmp(type,filetype)==0)
	{
		client_Req->type = FILETYPE_REQ;
		client_Req->length = htonl(strlen(filename));
		
		memset(buf,0,MAXLINE);
		pos = buf;

		memcpy(buf,client_Req,sizeof(struct msg_head));
		pos += sizeof(struct msg_head);

		memcpy(pos,filename,strlen(filename));
		pos += strlen(filename);	
	}
	else if(strcmp(type,checksum)==0)
	{
		client_Req->type = CHECKSUM_REQ;
		client_Req->length = htonl(strlen(filename));
		
		memset(buf,0,MAXLINE);
		pos = buf;

		memcpy(buf,client_Req,sizeof(struct msg_head));
		pos += sizeof(struct msg_head);

		memcpy(pos,&offset,sizeof(uint32_t));
		pos += sizeof(uint32_t);

		memcpy(pos,&len,sizeof(int32_t));
		pos += sizeof(int32_t);

		memcpy(pos,filename,strlen(filename));
		pos += strlen(filename);
	}
	else if(strcmp(type,download)==0)
	{
		client_Req->type = DOWNLOAD_REQ;
		client_Req->length = htonl(strlen(filename));

		memset(buf,0,MAXLINE);
		pos = buf;

		memcpy(buf,client_Req,sizeof(struct msg_head));
		pos += sizeof(struct msg_head);

		memcpy(pos,&offset,sizeof(uint32_t));
		pos += sizeof(uint32_t);

		memcpy(pos,&len,sizeof(int32_t));
		pos += sizeof(int32_t);

		memcpy(pos,filename,strlen(filename));
		pos += strlen(filename);
	}
	else 
	{
		usage();
	}

	sockfd=Socket(AF_INET,SOCK_STREAM,0);
		
	struct sockaddr_in servaddr;
    //置字节字符串s的前n个字节为零且包括‘\0’
	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family=AF_INET;
	servaddr.sin_port=htons(port);
    //hostent是host entry的缩写，该结构记录主机的信息，包括主机名、别名、地址类型、地址长度和地址列表。之所以主机的地址是一个列表的形式，原因是当一个主机有多个网络接口时，自然有多个地址。
	struct hostent *hent;
    //gethostbyname()成功时返回一个指向结构体 hostent 的指针，或者是个空（NULL）指针
	hent = gethostbyname(host);
	if(hent==NULL)
	{
		printf("Unknown host: %s\n", host);
		exit(1);
	}
    //in_addr是一个结构体，可以用来表示一个32位的IPv4地址。
	struct in_addr ia;
	memcpy(&ia.s_addr, hent->h_addr_list[0], sizeof(ia.s_addr));
	servaddr.sin_addr = ia;

	Connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	
	while(1)
	{
		Writen(sockfd,buf,MAXLINE);

		n=Readn(sockfd,recvline,sizeof(struct msg_head));
		struct msg_head* recvdata = (struct msg_head*)recvline;
		data_len = ntohl(recvdata->length);
		switch((unsigned char)recvdata->type)
		{
			case FILETYPE_RSP:
				n=Readn(sockfd,data,data_len);
				data[n]='\0';
				for(i=0;i<n;i++)
				{
					if((unsigned char)data[i]<=0x7f)
						continue;
					else 
					{
						printf("Invalid characters detected in a FILETYPE_RSP message.\n");
						goto done;
					}
				}
				printf("%s",data);
				goto done;
			case FILETYPE_ERR:
				printf("FILETYPE_ERR received and data length is %d\n",ntohl(recvdata->length));
				goto done;
			case CHECKSUM_RSP:
                if(data_len!=16){
                    printf("Invalid DataLength detected in a CHECKSUM_RSP message.\n");
                    goto done;
                }
				n=Readn(sockfd,data,data_len);
				for(i=0;i<16;i++)
				{
					printf("%02x",(unsigned char)data[i]);
				}
				printf("\n");
				goto done;
			case CHECKSUM_ERR:
				printf("CHECKSUM_ERR received and data length is %d\n",ntohl(recvdata->length));
				goto done;
			case DOWNLOAD_RSP:
				if(savefilepath == NULL)	
				{
                    //getcwd()会将当前工作目录的绝对路径复制到参数buffer所指的内存空间中,参数maxlen为buffer的空间大小,char *getcwd( char *buffer, int maxlen );
					getcwd(saveasfile,512);
                    //strrchr() 函数（在php中）查找字符在指定字符串中从左面开始的最后一次出现的位置，如果成功，返回该字符以及其后面的字符，如果失败，则返回 NULL
					savefilename = strrchr(filename,'/');
                    //把src所指向的字符串（包括“\0”）复制到dest所指向的字符串后面，extern char *strcat(char *dest, const char *src)
					strcat(saveasfile,savefilename);
					
					savefilepath = saveasfile;
					savefilename += 1;
                    //fopen函数是在当前目录下打开一个文件
					if((file = fopen(savefilepath,"w+"))==NULL)  
					{
						perror("fopen error\n");
					}
				}
				else 
				{
                    //access函数功能: 确定文件或文件夹的访问权限，F_OK 只判断是否存在
					if((access(savefilepath,F_OK))!=-1)	  
					{
						printf("File %s already exists,would you like to overwrite it? [yes/no](n)",savefilepath);
						scanf("%s",userinput);  
						if(userinput[0]=='y')
						{
							file = fopen(savefilepath,"w");
						}
						else if(userinput[0]=='n')  
						{
							goto done;
						}
						else
						{
							printf("Download canceled per user's request.\n");
							goto done;
						}
					}
					else
					{
						if((file = fopen(savefilepath,"w+"))==NULL)
						{
							perror("fopen error");
							goto done;
						}
					}
					savefilename = strrchr(savefilepath,'/');
					savefilename += 1;
				}

				s = data_len - 16;
				while(s > MAXLINE)
				{
					n = Readn(sockfd,data,MAXLINE);
                    //size_t fwrite(const void* buffer, size_t size, size_t count, FILE* stream)
                    //buffer：是一个指针，对fwrite来说，是要获取数据的地址,size：要写入内容的单字节数,count:要进行写入size字节的数据项的个数,stream:目标文件指针;
					fwrite(data,1,n,file);
					s = s-n;
				}
				n = Readn(sockfd,data,s);
				fwrite(data,1,n,file);

				n = Readn(sockfd,data,16);
				printf("...Downloaded data have been successfully written into '%s'(MD5=",savefilename);
				for(i=0;i<16;i++)
				{
					printf("%02x",(unsigned char)data[i]);
				}
				printf(")\n");

				fclose(file);
				goto done;
			case DOWNLOAD_ERR:
				printf("DOWNLOAD_ERR received and data length is %d\n",ntohl(recvdata->length));
				goto done;
			case UNKNOWN_FAIL:
				printf("UNKNOWN_FAIL received and data length is %d\n",ntohl(recvdata->length));
				goto done;
			default:
				goto done;
		}

	}
	done:
		Close(sockfd);

	return 0;			
}
