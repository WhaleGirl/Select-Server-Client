#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/select.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<string.h>

int fdArray[sizeof(fd_set)*8];
int startup(int port)
{
	int sock = socket(AF_INET,SOCK_STREAM,0);
	if(sock<0){
		perror("socket");
		exit(2);
	}
	//为了防止服务器主动断开连接，无法连接重启的问题
	int opt = 1;
	setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

	struct sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = htonl(INADDR_ANY);
	local.sin_port = htons(port);
	if(bind(sock,(struct sockaddr*)&local,sizeof(local))<0)
	{
		perror("bind");
		exit(3);
	}
	if(listen(sock,5)<0)
	{
		perror("listen");
		exit(4);
	}
	return sock;
}
// ./select_server 8080
int main(int argc,char* argv[])
{
	if(argc!=2)
	{
		printf("Usage: %s port\n",argv[0]);
		return 1;
	}
	int listen_sock = startup(atoi(argv[1]));
	//已经创建listen_sock	
	fdArray[0] = listen_sock;//合法的描述符
	int num = sizeof(fdArray)/sizeof(fdArray[0]);
	int i = 1;
	for(i;i<num;i++)
	{
		fdArray[i] = -1;
	}
	while(1)
	{
		fd_set rfds;
		FD_ZERO(&rfds);//清空文件描述符集
		//查找所有文件描述符集最大的
		int max = fdArray[0];//永远不会出错，因为第一个永远是有效的
		for(i = 0;i<num;i++)
		{
			//将fdArray中所有有效的文件描述符设置进了文件描述符集rfds
			if(fdArray[i]>=0)
			{
				//这就是合法的描述符
				FD_SET(fdArray[i],&rfds);
				if(max<fdArray[i]){
				  max = fdArray[i];
				}
			}
		}
		struct timeval timeout = {5,0};
		switch(select(max+1,&rfds,NULL,NULL,&timeout))
		{
			case 0:
				printf("timeout\n");
				break;
			case -1:
				perror("select");
				break;
			default:
				//走到这里，说明至少有一个文件描述符上的事件已经就绪了
				
				{
					for(i=0;i<num;i++)
					{
						
						if(fdArray[i]==-1){
							continue;
						}
						if(i==0 && fdArray[i]==listen_sock && FD_ISSET(fdArray[i],&rfds)){
							//已知哪个文件描述符就绪了并且能够获得新连接

							struct sockaddr_in client;
							socklen_t len = sizeof(client);
							int new_sock = accept(listen_sock,(struct sockaddr*)&client,&len);
							if(new_sock<0){
								perror("accept");
								continue;
							}			

							//不敢被阻塞,
							//添加到数组里
							for(i=0;i<num;i++)
							{
								if(fdArray[i]==-1){
									break;
								}
							}
							//找到一个没有被使用的位置
							if(i<num){
								fdArray[i] = new_sock;
							}//如果没有被添加说明数组满了，不能再处理新连接了
							else{
								close(new_sock);
							}

							//新连接要么已经放到数组里要么被关闭了
							continue;
						}
						//走到这里说明是普通的读事件就绪
						if(FD_ISSET(fdArray[i],&rfds))
						{
							char buf[1024] = {0};
							ssize_t s = read(fdArray[i],buf,sizeof(buf)-1);
							if(s==0){
								printf("client quit");
								close(fdArray[i]);//关闭当前的文件描述符
								fdArray[i]=-1;//并且将数组里的值设为-1，不然下次可能会再被读到
							}
							if(s<0){
								perror("read");
								close(fdArray[i]);
								fdArray[i]=-1;
								break;
							}
							printf("client: %s\n",buf);
							write(fdArray[i],buf,strlen(buf));
						}
					}
				}
				break;
		}
	}

	return 0;	
}
