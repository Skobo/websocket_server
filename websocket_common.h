
#ifndef _WEBSOCKET_COMMON_H_
#define _WEBSOCKET_COMMON_H_

#include <stdbool.h>

//#define WEBSOCKET_DEBUG
#define  SOUHU_DEBUG
// websocket根据data[0]判别数据包类型    比如0x81 = 0x80 | 0x1 为一个txt类型数据包
typedef enum{
    WDT_MINDATA = -20,      // 0x0：标识一个中间数据包
    WDT_TXTDATA = -19,      // 0x1：标识一个txt类型数据包
    WDT_BINDATA = -18,      // 0x2：标识一个bin类型数据包
    WDT_DISCONN = -17,      // 0x8：标识一个断开连接类型数据包
    WDT_PING = -16,     // 0x8：标识一个断开连接类型数据包
    WDT_PONG = -15,     // 0xA：表示一个pong类型数据包
    WDT_ERR = -1,
    WDT_NULL = 0
}WebsocketData_Type;


//====用户链表====
struct user_node{
    int fd;
    int id;
    int room;
    char *username;
   struct user_node *next;
};
//===房间链表===
struct room_node{
    int room;
    struct user_node *user_node_head;
    struct user_node *user_node_tail;
    struct room_node *next;
};

void del_room_user(int dfd);//删除链表某个节点
void send_room_user(int roomV,int idV,char* out);//给链表里面的成员进行推送

int regexp_pd(char *buf,const char *pattern);
int regexp(char *bematch,const char *pattern,char **val);//正则头文件

struct room_node* create_room_node(int room,int fd,int id,char *username);//创建房间节点
struct room_node* pdroom(int room,struct room_node* rh);//判断房间是否存在
void printf_room_node_user(struct room_node* p);//====输出房间的所有用户及用户名


int webSocket_clientLinkToServer(char *ip, int port, char *interface_path);//客户端链接server
int webSocket_serverLinkToClient(int fd, char *recvBuf, int bufLen);//服务器链接客户端

int webSocket_send(int fd, char *data, int dataLen, bool isMask, WebsocketData_Type type);//发送
int webSocket_recv(int fd, char *data, int dataMaxLen, WebsocketData_Type *dataType);//读

void webSocket_delayms(unsigned int ms);

// 其它工具
int websocket_getIpByHostName(char *hostName, char *backIp);//域名转 IP
int netCheck_setIP(char *devName, char *ip);
void netCheck_getIP(char *devName, char *ip);//设置和获取本机IP  *devName设备名称如: eth0

#endif

