//
// Created by chen on 2022/6/27.
//

#ifndef MYWEBSERVER_HTTP_COON_H
#define MYWEBSERVER_HTTP_COON_H

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <algorithm>
#include <iostream>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <errno.h>
#include <cstring>
#include <sys/uio.h>
#include <sys/mman.h>
#include <stdarg.h>

class http_coon {
public:
	static const int FILENAME_LEN = 200;
	static const int READ_BUFFER_SIZE = 2048;
	static const int WRITE_BUFFER_SIZE = 1024;
	enum METHOD
	{
		GET = 0,
		POST,
		HEAD,
		PUT,
		DELETE,
		TRACE,
		OPTIONS,
		CONNECT,
		PATH
	};
	enum CHECK_STATE
	{
		CHECK_STATE_REQUEST_LINE = 0,
		CHECK_STATE_HEADER,
		CHECK_STATE_CONTENT
	};
	enum HTTP_CODE
	{
		NO_REQUEST,
		GET_REQUEST,
		BAD_REQUEST,
		NO_RESOURCE,
		FORBIDDEN_REQUEST,
		FILE_REQUEST,
		INTERNAL_ERROR,
		CLOSED_CONNECTION
	};
	enum LINE_STATUS
	{
		LINE_OK = 0,
		LINE_BAD,
		LINE_OPEN
	};
public:
	http_coon();
	~http_coon();
	//新连接 初始化
	void init(int, const sockaddr_in&);
	//关闭连接fd
	void close_fd();
	//ET模式下 非阻塞读
	bool read();
	/**
	 * ET模式下 非阻塞写
	 * @return true:说明长连接不用关闭连接  false:可能是 出错 可能是 短连接 要关闭连接
	 */
	bool write();

	//http请求解析
	HTTP_CODE httpParse();
	//http响应填充
	bool fillResponse(HTTP_CODE);
private:
	//旧连接 初始化
	void init();
	/*
	 * http请求解析 所用到的函数
	 */
	//每一行的的解析
	LINE_STATUS lineParse();
	//解析请求行
	HTTP_CODE requestLineParse(char *);
	//解析头部字段
	HTTP_CODE headersParse(char *);
	//解析请求体 body
	HTTP_CODE contentParse(char *);
	//请求解析完成后 执行请求 检查请求资源 加载请求资源
	HTTP_CODE doRequest();
	//获取每一行
	char *getLine();

	void unmap();

	/*
	 * http响应填充 所用到的函数
	 */
	bool addResponse(const char* format, ...);
	bool addStatusLine(int, const char *);
	bool addHeaders(int);
	bool addContentLength(int);
	bool andLinger();
	bool addBlankLine();
	bool addContent(const char*);
public:
	//每个连接的fd
	int sock_fd;
	//每个连接的地址
	struct sockaddr_in address;
	//所有连接 都放在同一个 epoll fd上 用静态变量保存一下
	static int ep_fd;
	bool canWrite;
private:
	char readBuff[READ_BUFFER_SIZE];     //读缓冲空间
	int readIdx;           //已近读了的最后一个字节的下一个下标
	int checkedIdx;        //正在检查到什么位置的下标
	int startIdx;          //新的一行从哪个下标开始

	char writeBuff[WRITE_BUFFER_SIZE];    //写缓冲空间
	int writeIdx;       //已近写入写缓冲的下一个字节的下标 即缓冲区要发送的字节数

	int bytesToSend;    //需要发送的字节数
	int bytesHaveSend;  //已近发送的字节数

	CHECK_STATE checkState; //主状态机检查的状态
	METHOD method;          //请求方法


	//web服务器的根目录
	static const char* docRoot;
	//请求文件
	std::string fileName;
	//消息体内容
	char *m_content;
	//使用的http协议
	std::string version;
	//是否保持连接
	bool keepAlive;

	//存储 请求头部的 键值对
	std::unordered_map<std::string, std::string> header;
	//存储 post消息体中的键值对
	std::unordered_map<std::string, std::string> post;

	//文件存储的内存空间 起始地址
	char *fileAddress;
	//文件属性
	struct stat fileStat;
	//用于 writev写入发送缓冲区
	struct iovec iov[2];

	int iovCount;
};


/**
 * 设置文件非阻塞
 * @param fd 文件描述符
 */
void setNonBlock(int fd);
/**
 * 向epoll添加fd 设置 事件触发 已经添加的EPOLLIN | EPOLLET | EPOLLRDHUP
 * @param ep epoll的文件描述符
 * @param fd 需要监听的文件描述符
 * @param oneShot 是否开启 EPOLLONESHOT
 */
void fdAdd(int ep, int fd, bool oneShot);

/**
 * 在epoll中移除 文件描述符 并关闭连接
 * @param ep epoll的文件描述符
 * @param fd 需要移除监听的文件描述符
 */
void fdRemove(int ep, int fd);

/**
 * 向epoll 重新修改 fd事件 已经添加的EPOLLET | EPOLLONESHOT | EPOLLRDHUP
 * @param ep epoll的文件描述符
 * @param fd 需要修改监听的文件描述符
 * @param ev 监听事件
 */
void fdMode(int ep, int fd, int ev);

#endif //MYWEBSERVER_HTTP_COON_H
