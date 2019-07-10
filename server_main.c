
#include "websocket_common.h"


#include <stdio.h>
#include <stdlib.h>     //exit()
#include <string.h>
#include <errno.h>
#include <fcntl.h>      // 非阻塞宏
#include <sys/ioctl.h>
#include <sys/epoll.h>  // epoll管理服务器的连接和接收触发
#include <sys/socket.h>
#include <netinet/ip.h>
#include <pthread.h>    // 使用多线程
#include <hiredis/hiredis.h>  
#include <unistd.h>
#define		EPOLL_RESPOND_NUM		10000	// epoll最大同时管理句柄数量
// #define     REDIS_IP "127.0.0.1"    //redis ip
// #define     REDIS_PORT 6379         //redis port
//#define     WEBSOCKET_SERVER_PORT 8080
#define     KEY 5566
#include "cJSON.h"
#include <regex.h> //正则头文件
#include "config.h"  
#include <sys/stat.h> 
/*
*  作者: lx
*/
typedef int (*CallBackFun)(int fd, char *buf, unsigned int bufLen);
redisContext* pubconn;
redisContext* subconn;

extern struct room_node* RoomHeadNode;
extern struct room_node* RoomCurrentNode;
typedef struct{
    int fd;
    int client_fd_array[EPOLL_RESPOND_NUM][2];
    char ip[24];
    int port;
    char buf[10240];
    CallBackFun callBack;
}WebSocket_Server;


// Tool Function
int arrayAddItem(int array[][2], int arraySize, int value)
{
    int i;
    for(i = 0; i < arraySize; i++)
    {
        if(array[i][1] == 0)
        {
            array[i][0] = value;
            array[i][1] = 1;
            return 0;
        }
    }
    return -1;
}

int arrayRemoveItem(int array[][2], int arraySize, int value)
{
    int i;
    for(i = 0; i < arraySize; i++)
    {
        if(array[i][0] == value)
        {
            array[i][0] = 0;
            array[i][1] = 0;
            return 0;
        }
    }
    return -1;
}


// Server Function
void server_thread_fun(void *arge)
{
	int ret , i , j;
	int accept_fd;
	int socAddrLen;
	struct sockaddr_in acceptAddr;
	struct sockaddr_in serverAddr;
	//
	WebSocket_Server *wss = (WebSocket_Server *)arge;
	//
	memset(&serverAddr , 0 , sizeof(serverAddr)); 	// 数据初始化--清零     
    serverAddr.sin_family = AF_INET; 				// 设置为IP通信     
    //serverAddr.sin_addr.s_addr = inet_addr(wss->ip);// 服务器IP地址
    serverAddr.sin_addr.s_addr = INADDR_ANY;		// 服务器IP地址
    serverAddr.sin_port = htons(wss->port); 		// 服务器端口号    
	//
	socAddrLen = sizeof(struct sockaddr_in);
	
	//------------------------------------------------------------------------------ socket init
	//socket init
	wss->fd = socket(AF_INET, SOCK_STREAM,0);  
    if(wss->fd <= 0)  
    {  
        printf("server cannot create socket !\r\n"); 
        exit(1);
    }    
    
    //设置为非阻塞接收
    ret = fcntl(wss->fd , F_GETFL , 0);
    fcntl(wss->fd , F_SETFL , ret | O_NONBLOCK);
    
    //bind sockfd & addr  
    while(bind(wss->fd, (struct sockaddr *)&serverAddr, sizeof(struct sockaddr)) < 0 )
        webSocket_delayms(1);
    
    //listen sockfd   
    ret = listen(wss->fd, 128);  
    if(ret < 0) 
    {  
        printf("server cannot listen request\r\n"); 
        close(wss->fd); 
        exit(1);
    } 
    //------------------------------------------------------------------------------ epoll init
     // 创建一个epoll句柄  
    int epoll_fd;  
    epoll_fd = epoll_create(EPOLL_RESPOND_NUM);  
    if(epoll_fd < 0)
    {  
        printf("server epoll_create failed\r\n"); 
        exit(1);
    }  
    
    int nfds;                     // epoll监听事件发生的个数  
    struct epoll_event ev;        // epoll事件结构体  
    struct epoll_event events[EPOLL_RESPOND_NUM];
    ev.events = EPOLLIN|EPOLLET;  // 	EPOLLIN		EPOLLET;    监听事件类型
    ev.data.fd = wss->fd;  
    // 向epoll注册server_sockfd监听事件  
    if(epoll_ctl(epoll_fd , EPOLL_CTL_ADD , wss->fd , &ev) < 0)
    {  
        printf("server epll_ctl : wss->fd register failed\r\n"); 
        close(epoll_fd);
        exit(1);
    }  
    
	//------------------------------------------------------------------------------ server receive
	printf("\r\n\r\n========== server start ! ==========\r\n\r\n");
    while(1)
    { 
    	// 等待事件发生  
        nfds = epoll_wait(epoll_fd , events , EPOLL_RESPOND_NUM , -1);  	// -1表示阻塞、其它数值为超时
        if(nfds < 0)
        {  
            printf("server start epoll_wait failed\r\n"); 
            close(epoll_fd);
            exit(1);
        }  
       
        for(j = 0 ; j < nfds ; j++)
        { 
        	//===================epoll错误 =================== 
            if ((events[j].events & EPOLLERR) || (events[j].events & EPOLLHUP))  
            {
            	    //printf("server epoll err !\r\n"); 
            	    //exit(1);
            	    printf("accept close : %d\r\n", events[j].data.fd);     // 与客户端连接出错, 主动断开当前 连接
					// 向epoll删除client_sockfd监听事件  
					//ev.events = EPOLLIN|EPOLLET;
        			ev.data.fd = events[j].data.fd; 
				    if(epoll_ctl(epoll_fd , EPOLL_CTL_DEL , events[j].data.fd , &ev) < 0) 
				    {  
				        printf("server epoll_ctl : EPOLL_CTL_DEL failed !\r\n"); 
				        close(epoll_fd);
				        exit(1);
				    }
				    arrayRemoveItem(wss->client_fd_array, EPOLL_RESPOND_NUM, events[j].data.fd);    // 从数组剔除fd
                    //删除链表某个节点
					close(events[j].data.fd);	//关闭通道
            }
        	//===================新通道接入事件===================
        	else if(events[j].data.fd == wss->fd)//有用户链接过来,将fd加入监听,并且,添加到fd数组
        	{		
				//轮巡可能接入的新通道 并把通道号记录在accept_fd[]数组中
				accept_fd = accept(wss->fd, (struct sockaddr *)&acceptAddr, &socAddrLen);    
				if(accept_fd >= 0)  //----------有新接入，通道号加1----------
				{ 	
					// 向epoll注册client_sockfd监听事件  
					//ev.events = EPOLLIN|EPOLLET;
		    		ev.data.fd = accept_fd; 
					if(epoll_ctl(epoll_fd , EPOLL_CTL_ADD , accept_fd , &ev) < 0) //加入红黑树失败
					{  
						 printf("server epoll_ctl : EPOLL_CTL_ADD failed !\r\n"); 
						 close(epoll_fd);
						 exit(1);
					}
					//send(accept_fd , "OK\r\n" , 4 , MSG_NOSIGNAL);
					printf("server fd/%d : accept\r\n", accept_fd);
					arrayAddItem(wss->client_fd_array, EPOLL_RESPOND_NUM, accept_fd);    // 添加fd到数组
                   // printf("断点调试\n");
				}
			}
			//===================接收数据事件===================
			else if(events[j].events & EPOLLIN)
			{		
				memset(wss->buf, 0, sizeof(wss->buf));
				ret = wss->callBack(events[j].data.fd , wss->buf , sizeof(wss->buf));
               // printf("返回的值<<<%d\n",ret);
				if(ret <= 0)		//----------ret<=0时检查异常, 决定是否主动解除连接----------
				{
				    if(errno == EAGAIN || errno == EINTR)//如果是服务器出现问题的话
                        ;
                    else
                    { 

					    printf("accept close : %d\r\n", events[j].data.fd);

                        //webSocket_send(events[j].data.fd , "{\"type\":\"error\",\"code\":404}", strlen("{\"type\":\"error\",\"code\":404}"), false, WDT_TXTDATA);
					    // 向epoll删除client_sockfd监听事件  
					    //ev.events = EPOLLIN|EPOLLET;
            			ev.data.fd = events[j].data.fd; 
				        if(epoll_ctl(epoll_fd , EPOLL_CTL_DEL , events[j].data.fd , &ev) < 0) 
				        {  
				            printf("server epoll_ctl : EPOLL_CTL_DEL failed !\r\n"); 
				            close(epoll_fd);
				            exit(1);
				        }
				        arrayRemoveItem(wss->client_fd_array, EPOLL_RESPOND_NUM, events[j].data.fd);    // 从数组剔除fd
                        //从链表中删除这个房间的用户
                         del_room_user(events[j].data.fd);
                         printf_room_node_user(RoomHeadNode);//输出所有房间及用户
					    close(events[j].data.fd);	//关闭通道
					}
				}
			}
			//===================发送数据事件===================
			else if(events[j].events & EPOLLOUT)
			    ;
		}
    }
	//关闭epoll句柄
    close(epoll_fd);
    //关闭socket
    close(wss->fd);
}

//
int server_callBack(int fd, char *buf, unsigned int bufLen)
{
    int ret;
    //printf("接收到请求要处理什么操作\n");
    ret = webSocket_recv(fd , buf , bufLen, NULL);    // 使用websocket recv 解析数数据包
    if(ret > 0)
	{
	    printf("server(fd/%d) recv: %s\r\n", fd, buf);
        const char *pattern = "^\\{\"room\":[0-9]+,\"id\":[0-9]+,\"username\":\".{3,20}\",\"content\":\".{1,100}\",\"to\":[0-9]+\\}$";
        //char *buf = "{\"rom\":1000,\"id\":77,\"username\":\"username\",\"content\":\"fdsagdsafdsafddddddddddddddddddddddddfdsa\",\"to\":0}";
        int d = regexp_pd(buf,pattern);
        if(d>0){//正则匹配数据格式 ，如果格式正确则执行 
        //===操作redis===订阅
            redisReply* replyd = redisCommand(pubconn, "publish foo %s",buf);
            if (replyd != NULL){
                if (replyd->type == REDIS_REPLY_ERROR){
                    printf("发布命令失败\n");
                }
            }else{
                printf("与redis服务器链接失败\n");
                
            }
             freeReplyObject(replyd);
        }else{
            printf("内容格式不正确\n");
            return -1;
        }
		//===== 在这里根据客户端的请求内容, 提供相应的服务 =====
		//if(strstr(buf, "hi~") != NULL)
		//   ret = webSocket_send(fd, "Hi~ I am server", strlen("Hi~ I am server"), false, WDT_TXTDATA);
	}
    else if(ret < 0)//小于0说明是建立链接的请求
    {
        if(strncmp(buf, "GET", 3) == 0){//握手,建立连接
            ret = webSocket_serverLinkToClient(fd, buf, ret);
        }	
    }
	return ret;
}


//====第二个子线程的逻辑就是订阅redis 然后收到消息循环发送给所有的用户=====
void fun_redis_sub(){
        //printf("开始执行sub逻辑\n");

       //===== 执行线程的逻辑===== 
        redisReply* reply = redisCommand(subconn,"SUBSCRIBE foo");
        freeReplyObject(reply);
        while(redisGetReply(subconn,(void**)&reply) == REDIS_OK) {
            switch(reply->type){
                case REDIS_REPLY_STRING : //1 表示是字符串
                    printf("1\n");
                    printf("%d\n",reply->elements);
                
                    break;
                case REDIS_REPLY_ARRAY : //2 表示返回的是数组
                    
                    printf("%s\n", reply->element[2]->str); 
                    cJSON *json,*json_to,*json_room;
                    char* out = reply->element[2]->str;
 
                    json = cJSON_Parse(out); //解析成json形式
                    if(!json){
                        break;
                    }
                    json_to = cJSON_GetObjectItem( json , "to" );  //获取键值内容 数值类型
                    if(!json_to){
                        break;
                    }
                    json_room = cJSON_GetObjectItem( json , "room" );//int型
                    if(!json_room){
                        break;
                    }
                     //--此处进行消息推送
                    send_room_user(json_room->valueint,json_to->valueint,out);
                    cJSON_Delete(json);  //释放内存 
                    //free(out);
                    break;
                case REDIS_REPLY_INTEGER : //3 表示返回的是数字
                    printf("3\n");
                    printf("%d\n",reply->elements);
                    break;
                case REDIS_REPLY_NIL :
                    break;
                case REDIS_REPLY_STATUS :
                    break;
                case REDIS_REPLY_ERROR :
                    break;
                default :
                    printf("其它类型\n");
                    break;
                
            }
        
            // consume message
            freeReplyObject(reply);
        }

}
// 守护进程初始化函数
void init_daemon()
{
    pid_t pid;
    int i = 0,op;

    if ((pid = fork()) == -1) {
        printf("Fork error !\n");
        exit(1);
    }
    if (pid != 0) {
        exit(0);        // 父进程退出
    }

    setsid();           // 子进程开启新会话，并成为会话首进程和组长进程
    if ((pid = fork()) == -1) {
        printf("Fork error !\n");
        exit(-1);
    }
    if (pid != 0) {
        exit(0);        // 结束第一子进程，第二子进程不再是会话首进程
    }
    chdir("/");      // 改变工作目录
    umask(0);           // 重设文件掩码
    for (; i < getdtablesize(); ++i) {
       close(i);        // 关闭打开的文件描述符
    }
    //close(STDIN_FILENO);//关闭屏幕的输入流 STDIN_FILENO 是0 STDOUT_FILENO是2 STDERR_FILENO 是3
	// open("./log.txt",O_RDWR);//打开黑洞文件 返回的文件描述符从最小的开始，就是o
    // op =  open("./log.txt",O_RDWR);
	// dup2(op,STDOUT_FILENO);  //将标准输出重定向到黑洞文件
	// dup2(op,STDERR_FILENO);  //将标准错误 重定到的黑洞文件

    close(STDIN_FILENO);//关闭屏幕的输入流 STDIN_FILENO 是0 STDOUT_FILENO是2 STDERR_FILENO 是3
	open("/dev/null",O_RDWR);//打开黑洞文件 返回的文件描述符从最小的开始，就是o
	dup2(0,STDOUT_FILENO);  //将标准输出重定向到黑洞文件
	dup2(0,STDERR_FILENO);  //将标准错误 重定到的黑洞文件
    return;
}
int main()
{

   
    printf("websocket_server start ok\n");
    
    pid_t pid[2];
    int f,mywait,mywaitstatus;
    char redisIp[16]; 
    char port[10];  
    int redisPort; 
    int websocketServerPort;
    GetProfileString("./websocket.conf", "redis", "ip", redisIp);
    GetProfileString("./websocket.conf", "redis", "port", port);
    redisPort = atoi(port);
    GetProfileString("./websocket.conf", "websocket_server", "port", port);
    websocketServerPort = atoi(port);
    // printf("redis_ip====%s====\n",redisIp);
    // printf("redis_port====%d====\n",redisPort);
    // printf("websocket_server_port====%d====\n",websocketServerPort);
    // exit(-1);


    /*
 	*   守护进程
 	*
 	*/
    init_daemon();

    //===链接redis==== 这个redis文件描述符用来推送
    pubconn = redisConnect(redisIp, redisPort);  //链接redis
    if(pubconn->err)   printf("connection error:%s\n", pubconn->errstr);  
    //===链接redis==== 这个redis文件描述符用来接收
    subconn = redisConnect(redisIp, redisPort);  //链接redis
    if(subconn->err)   printf("connection error:%s\n", subconn->errstr);  

    // //======循环创建2个子进程======
    // for(f=0;f<2;f++){
    //     pid[f] = fork();
    //     if(pid[f]==0){
	// 		break;
	// 	}
	// 	if(pid[f]<0){
	// 		perror("fork error");	
	// 	}
    // }
    

//=========下面是websocket====服务====
        int exitFlag;
        int i, client_fd;
        pthread_t sever_thread_id,server_thread_sub_id;
        WebSocket_Server wss;
           //===== 初始化服务器参数 =====
        memset(&wss, 0, sizeof(wss));
        //strcpy(wss.ip, "127.0.0.1");     
        wss.port = websocketServerPort;
        wss.callBack = &server_callBack; // 响应客户端时, 需要干嘛?

        
        //===== 开辟线程, 管理websocket服务器 =====
        if(pthread_create(&sever_thread_id, NULL, (void*)&server_thread_fun, (void *)(&wss)) != 0)
        {
            printf("create server false !\r\n");
            exit(1);
        } 
        //自动让线程退出
        pthread_detach(sever_thread_id);
         if(pthread_create(&server_thread_sub_id, NULL, (void*)&fun_redis_sub, NULL) != 0)
        {
            printf("create server false !\r\n");
            exit(1);
        } 
        pthread_detach(server_thread_sub_id);
        //
        exitFlag = 0;
        while(!exitFlag)
        {
            // 每3秒推送信息给所有客户端
            for(i = 0; i < EPOLL_RESPOND_NUM; i++)
            {
                if(wss.client_fd_array[i][1] != 0 && wss.client_fd_array[i][0] > 0)
                {
                    client_fd = wss.client_fd_array[i][0];
                    if(webSocket_send(client_fd, "{\"room\":\"1000\",\"username\":\"humingming\"}", strlen("{\"room\":\"1000\",\"username\":\"humingming\"}"), false, WDT_TXTDATA) < 0)
                    {
                        printf("server webSocket_send err !!\r\n");
                        exitFlag = 1;
                        break;
                    }
                }
            }
            webSocket_delayms(3000);
        }
        
        //==============================
        redisFree(subconn); 
        redisFree(pubconn); 
    
   //{"room":1000,"id":77,"username":"username","content":"fdsagdsafdsafddddddddddddddddddddddddfdsa","to":0}
 
	
    return 0;
}







