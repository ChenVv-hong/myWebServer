//
// Created by chen on 2022/7/3.
//

#include "server.h"

//用于信号传输的管道 由epoll监听 统一事件源
int pipe_fd[2];

void *thrFunc(void *arg) {
	http_coon *user = (http_coon *)arg;
	http_coon::HTTP_CODE ret = user->httpParse();
	if(ret == http_coon::NO_REQUEST){
		fdMode(user->ep_fd, user->sock_fd, EPOLLIN);
		return nullptr;
	}
	if(!user->fillResponse(ret)){
		//不直接关闭 将canWrite变为false 在 write失败时再关闭
//		fdRemove(user->ep_fd, user->sock_fd);
		user->canWrite = false;
	}
	user->canWrite = true;
	fdMode(user->ep_fd, user->sock_fd, EPOLLOUT);
	return nullptr;
}

void cbFunc(client_data *c) {
	//连接超时 从epoll上摘除 并关闭连接
	fdRemove(c->ep_fd, c->sock_fd);
}

void sig_handle(int sig) {
	//将触发的信号 写入管道 等待epoll来正式处理
	int save_errno = errno;
	int msg = sig;
	send(pipe_fd[1], (char *)&msg, 1, 0);
	errno = save_errno;
}

server::server(char *ip, int port) {
	strcpy(this->IP, ip);
	this->PORT = port;
	this->address.sin_port = htons(this->PORT);
	inet_pton(AF_INET,this->IP,&this->address.sin_addr.s_addr);
	this->address.sin_family = AF_INET;

	this->isRun = false;
	this->timeout = false;

	this->users = new http_coon[MAX_FD];
	this->cds = new client_data[MAX_FD];

	pool = new thread_pool(12,12,10000, false);
}

server::~server() {

	close(ep_fd);
	close(lfd);
	close(pipe_fd[1]);
	close(pipe_fd[0]);

	delete[] users;
	delete pool;
}

void server::start() {
	eventListen();
	this->isRun = true;
	int ret;
	while(isRun){
		int ready = epoll_wait(ep_fd, events, MAX_EVENT_NUMBER, -1);
		if(ready < 0 && errno != EINTR){
			std::cout << "epoll failure\n";
			break;
		}
		for(int i = 0; i < ready; i++){
			int sock_fd = events[i].data.fd;
			if(sock_fd == lfd){
				//新连接
				sockaddr_in client_address;
				socklen_t client_add_len;
				int conn_fd = accept(lfd, (sockaddr *)&client_address,&client_add_len);

//				char ip[INET_ADDRSTRLEN];
//				inet_ntop(AF_INET, &client_address.sin_addr.s_addr, ip, INET_ADDRSTRLEN);
//				std::cout << "client ip : " << ip << "\nport : " << ntohs(client_address.sin_port) << std::endl;
				//初始化新连接
				initNewConnection(conn_fd, client_address);
			}
			else if(sock_fd == pipe_fd[0]){
				//信号源
				if(events[i].events & EPOLLIN){
					int sig;
					char signals[1024];
					ret = recv(sock_fd, signals, sizeof signals, 0);
					if(ret == -1){
						//TODO 处理错误
						continue;
					}
					else if(ret == 0){}
					else{
						for(int i = 0; i < ret; i++){
							switch (signals[i]) {
								case(SIGALRM):{
									timeout = true;
									break;
								}
								case(SIGTERM):{
									this->isRun = false;
									break;
								}
							}
						}
					}
				}
				else if(events[i].events & EPOLLRDHUP){
					std::cout << "对端关闭\n";
					assert(false);
				}
				else{
					std::cout << "error pipe_fd[0]\n";
					assert(false);
				}
			}
			else{
				//客户端请求
				if(events[i].events & EPOLLIN){
					//活跃连接 更新定时器
					cds[sock_fd].t->expire = time(nullptr) + 3 * TIMESLOT;
					timeList.adjustTimer(cds[sock_fd].t);
					if(users[sock_fd].read()){
						//读出数据 供工作线程解析
						task t;
						t.arg = (void *)&users[sock_fd];
						t.func = thrFunc;
						pool->add_task(t);
					}
					else{
						//异常
						//为了保证服务器正常运行 对这个连接进行丢弃
						fdRemove(ep_fd, sock_fd);
						//删除定时器
						timeList.delTimer(cds[sock_fd].t);
					}
				}
				else if(events[i].events & EPOLLOUT){
					//活跃连接 更新定时器
					cds[sock_fd].t->expire = time(nullptr) + 3 * TIMESLOT;
					timeList.adjustTimer(cds[sock_fd].t);

					if(!users[sock_fd].write()){
						fdRemove(ep_fd, sock_fd);
						//删除定时器
						timeList.delTimer(cds[sock_fd].t);
					}
				}
				else if(events[i].events & EPOLLRDHUP){
					//对方关闭连接
					fdRemove(ep_fd, sock_fd);
					//删除定时器
					timeList.delTimer(cds[sock_fd].t);
				}
				else{
					//异常
					//为了保证服务器正常运行 对这个连接进行丢弃
					fdRemove(ep_fd, sock_fd);
					//删除定时器
					timeList.delTimer(cds[sock_fd].t);
				}
			}
		}
		if(timeout){
			timerHandle();
			timeout = false;
		}
	}
}

void server::stop() {

}

void server::eventListen() {
	this->lfd = socket(PF_INET, SOCK_STREAM, 0);
	int ret = bind(lfd, (sockaddr *)&this->address, sizeof(this->address));
	if(ret == -1){
		perror("bind error");
	}
	assert(ret != -1);

	ret = listen(lfd, MAX_LISTEN);
	assert(ret != -1);
	this->ep_fd = epoll_create(MAX_FD);
	assert(ep_fd != -1);
	http_coon::ep_fd = this->ep_fd;

//	fdAdd(this->ep_fd, lfd, false);
	epoll_event event;
	event.data.fd = lfd;
	event.events = EPOLLIN;
	epoll_ctl(ep_fd, EPOLL_CTL_ADD, lfd, &event);

	//统一事件源 将信号定时事件 都交给epoll监听
	ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipe_fd);
	assert(ret != -1);
	setNonBlock(pipe_fd[1]); //管道写端非阻塞
	fdAdd(ep_fd, pipe_fd[0], false);

	// 设置信号处理函数
	addSig(SIGALRM);
	addSig(SIGTERM);
	alarm(SIGALRM);
}

void server::initNewConnection(int fd, sockaddr_in & clientAddress) {
	//将socket 放入epoll 监听
	fdAdd(ep_fd, fd, true);
	//给一个连接对象
	users[fd].init(fd, clientAddress);
	//创建定时器 设置其回调函数与超时时间 绑定用户数据 同时加入定时器容器
	timer *t = new timer;
	t->expire = time(nullptr) + 3 * TIMESLOT;
	t->cb_func = cbFunc;
	t->user_data = &cds[fd];
	cds[fd].sock_fd = fd;
	cds[fd].t = t;
	//将新连接的定时任务 放入定时器容器
	this->timeList.addTimer(t);
}

void server::addSig(int sig) {
	struct sigaction sa;
	memset(&sa, '\0', sizeof sa);
	sa.sa_handler = sig_handle;
	sa.sa_flags |= SA_RESTART; //重新调用 被该信号中断的系统调用
	sigfillset(&sa.sa_mask);
	assert(sigaction(sig, &sa, nullptr) != -1);
}

void server::timerHandle() {
	this->timeList.tick();
	alarm(SIGALRM);
}







