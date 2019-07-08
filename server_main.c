
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
#define	    EPOLL_RESPOND_NUM		10000	// epoll最大同时管理句柄数量
#define     REDIS_IP "127.0.0.1"    //redis ip
#define     REDIS_PORT 6379         //redis port
#define     WEBSOCKET_SERVER_PORT 8080
#define     KEY 5566
#include "cJSON.h"
#include <regex.h> //正则头文件

/*
 *  作者：qq：769777107
 *  c语言 epoll网络模型的 websocket服务器
 *  利用redis 订阅和发布，做了横向集群
 *  支持定制功能,有需要请联系作者
 *
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
/* =====================
*   regexp_pd正则匹配，
*   buf 要匹配的字符串
*   pattern 正则字符串
*   成功返回大于0失败返回于小0
*/


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
                    printf("断点调试\n");
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
    //printf("1\n");
    if(ret > 0)
	{
        //printf("2\n");
	    printf("server(fd/%d) recv: %s\r\n", fd, buf);
        //printf("3\n");
       
        const char *pattern = "^\\{\"room\":[0-9]+,\"id\":[0-9]+,\"username\":\".{3,20}\",\"content\":\".{1,100}\",\"to\":[0-9]+\\}$";
        //printf("4\n");
        //char *buf = "{\"rom\":1000,\"id\":77,\"username\":\"liuxiang\",\"content\":\"fdsagdsafdsafddddddddddddddddddddddddfdsa\",\"to\":0}";
        int d = regexp_pd(buf,pattern);
        //printf("5\n");
        //redisReply* replyd = NULL;
        if(d>0){//正则匹配数据格式 ，如果格式正确则执行 
        //printf("6\n");
        //===操作redis===订阅
            redisReply* replyd = redisCommand(pubconn, "publish foo %s",buf);
            //printf("7\n");
            if (replyd != NULL){
                //printf("8\n");
                if (replyd->type == REDIS_REPLY_ERROR){
                    //printf("1\n");
                    printf("发布命令失败\n");
                }
                printf("9\n");
            }else{
                //printf("10\n");
                printf("与redis服务器链接失败\n");
                
            }
             freeReplyObject(replyd);
             //printf("11\n");
        }else{
            printf("内容格式不正确\n");
            return -1;
        }
       
        printf("12\n");
		//===== 在这里根据客户端的请求内容, 提供相应的服务 =====
		//if(strstr(buf, "hi~") != NULL)
		 //   ret = webSocket_send(fd, "Hi~ I am server", strlen("Hi~ I am server"), false, WDT_TXTDATA);
		
		// ... ...
		// ...
	}
    else if(ret < 0)
    {
        printf("13\n");
        if(strncmp(buf, "GET", 3) == 0){//握手,建立连接
            printf("14\n");
            ret = webSocket_serverLinkToClient(fd, buf, ret);
            printf("15\n");
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
                    struct room_node* srm = RoomHeadNode;
                    while(srm != NULL){
                        if(srm->room == json_room->valueint){
                            struct user_node* un = srm->user_node_head;
                            if(json_to->valueint == 0){
                                while(un != NULL){
                                    int ret;
                                    ret = webSocket_send(un->fd, out, strlen(out), false, WDT_TXTDATA);
                                    un = un->next;
                                }
                            }else if(json_to->valueint > 0){

                                while(un != NULL){
                                    if(un->id == json_to->valueint){
                                        int ret;
                                        ret = webSocket_send(un->fd, out, strlen(out), false, WDT_TXTDATA);
                                    }
                                    un = un->next;
                                }
                            }

                        }
                        srm = srm->next;
                    }
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

int main(void)
{
   
    //=== 初始化房间节点===



    pid_t pid[2];
    int f,mywait,mywaitstatus;

    //===链接redis====
    pubconn = redisConnect(REDIS_IP, REDIS_PORT);  //链接redis
    if(pubconn->err)   printf("connection error:%s\n", pubconn->errstr);  
 //===链接redis====
    subconn = redisConnect(REDIS_IP, REDIS_PORT);  //链接redis
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
        wss.port = WEBSOCKET_SERVER_PORT;
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
       // pthread_cancel(sever_thread_id);     // 等待线程关闭
        //printf("server close !\r\n");
        redisFree(subconn); 
        redisFree(pubconn); 
    
   //{"room":1000,"id":77,"username":"usrname","content":"fdsagdsafdsafddddddddddddddddddddddddfdsa","to":0}
    return 0;
}







