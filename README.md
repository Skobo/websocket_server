# websocket_server
   注意这个是linux下websocket，只声明一遍。
   个人集群websocket测试地址  
   ws://47.93.5.97:8081?room=1000&id=15&username=hm  
   用了两台服务器，其中一台，提供web及websocket及redis服务 另一台是香港的服务器，一台1G内存一台512内存  
   每个月都要花钱 可能指不定什么时候都关了

## 使用帮助<br>

### 1:安装redis<br>
   这个百度有很多答案，不在此细说了
   安装好了后，修改redis.conf bind 127.0.0.1 为 bind 0.0.0.0 再将 protected-mode yes，改为protected-mode no
  
   启动./bin/redis-server ./redis.conf   
### 2:安装hiredis<br>
   参考：https://www.cnblogs.com/houjun/p/4877052.html  
### 3:将代码拷贝到linux某一目录下
### 4:cd到代码目录
    修改websocket.conf 配置文件 修改方法配置文件已经中文注释
### 5:执行make命令
    注意：如果有错误，请自行百度，或者谷歌,实在不行，可以联系我，qq下面有提供
### 6:执行 ./websocket_server
    是以守护进程形式启动的，可以用 netstat -tunpl 查看系统开启动了那些服务
    默认是用8080端口，你可以在配置文

## 前端链接示例
ws://ip:8080?room=房间号&id=用户id&username=用户名 <br>
注意链接格式，如果格式不正确将连接不上。因为在程序里面用正则做了限制<br>
## 发送的文本格式是json字符串，格式如下
	{"room":1000,"id":77,"username":"username","content":"要发送的内容","to":0}

	to:0 表示给1000房间所有人发送<br>
	to:id 表示给房间指定用户发送

     
注意
--------
如果想要详细的集群部署方案，可以参考，nginx websocket 代理，将此代码部署到多台linux上。<br>

## nginx websocket集群配置
map $http_upgrade $connection_upgrade {<br>
    default upgrade;  
    '' close;  
}

upstream websocket {  
    server websocket_server 1的ip:8080;  
    server websocket_server12的ip:8080;  
}  

server {  
    listen 8080;  
    location /ws {  
        proxy_pass http://websocket;  
        proxy_http_version 1.1;  
        proxy_set_header Upgrade $http_upgrade;  
        proxy_set_header Connection $connection_upgrade;  
    }  

}  

增加配置文件启动<br>
如果你比较急，想使用集群方案，扩展弹幕系统性能，或者聊天性能，可以联系我qq：769777107
