//
// Created by chen on 2022/7/3.
//

#ifndef MYWEBSERVER_SERVER_H
#define MYWEBSERVER_SERVER_H

#include "../http/http_coon.h"
#include "../threadpool/thread_pool.h"
#include "../timer/timer.h"
#include <signal.h>
#include <assert.h>

#define MAX_FD 65535
#define MAX_LISTEN 5
#define MAX_EVENT_NUMBER 1024

void sig_handle(int sig);
void cbFunc(client_data *c);
void * thrFunc(void *);

class server {
public:

public:
	server(char *ip, int port);
	~server();
	server(const server&)=delete;
	server& operator=(const server&)=delete;
	void start();
	void stop();
	/**
	 * 线程函数
	 */
	friend void * thrFunc(void *);
	/**
	 * 定时器任务
	 * @param c 用户数据
	 */
	friend void cbFunc(client_data *c);
	/**
	 * 信号处理函数
	 * @param sig 信号
	 */
	friend void sig_handle(int sig);
private:
	/*
	 * 服务器的网络准备工作
	 */
	void eventListen();
	/**
	 * 对新连接进行初始化
	 * @param fd 新连接的文件描述符
	 * @param addr 客户端地址
	 */
	void initNewConnection(int fd, sockaddr_in &addr);
	/**
	 * 添加监听信号
	 * @param isg
	 */
	void addSig(int sig);
	/**
	 * 处理定时器任务 并重新定时
	 */
	void timerHandle();
private:
	/**
	 * 服务端ip地址 端口号
	 */
	char IP[INET_ADDRSTRLEN];
	int PORT;
	sockaddr_in address;
	int isRun;
	//监听socket
	int lfd;
	//epoll
	int ep_fd;
	//客户连接
	http_coon *users;
	client_data *cds;

	bool timeout;

	/**
	 * 就绪事件数组
	 */
	epoll_event events[MAX_EVENT_NUMBER];


	//线程池
	thread_pool *pool;

	//定时器容器
	sort_time_list timeList;

};



#endif //MYWEBSERVER_SERVER_H
