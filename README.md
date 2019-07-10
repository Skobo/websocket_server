# websocket_server


#使用帮助<br>

1:安装redis<br>
2:安装hiredis<br>
3:将代码拷贝到linux某一目录下<br>
4:cd到代码目录<br>
5:执行make命令（注意：如果有错误，请自行百度，或者谷歌）<br>
6:执行 ./websocket_server<br>

#查看进程状态
netstat -tunpl<br>
默认是用8080端口，你可以在配置文件更改 

#前端链接示例
ws://ip:8080?room=房间号&id=用户id&username=用户名 <br>
注意链接格式，如果格式不正确将连接不上。因为在程序里面用正则做了限制<br>
#发送的文本格式是json字符串，格式如下
	{\"room\":1000,\"id\":77,\"username\":\"username\",\"content\":\"要发送的内容\",\"to\":0}<br>

	to:0 表示给1000房间所有人发送<br>
	to:id 表示给房间指定用户发送

     
注意
--------
如果想要详细的集群部署方案，可以参考，nginx websocket 代理，将此代码部署到多台linux上。<br>
后续我会添加 <br>
nginx websocket集群代理配置。<br>
增加配置文件启动<br>
如果你比较急，想使用集群方案，扩展弹幕系统性能，或者聊天性能，可以联系我qq：769777107
