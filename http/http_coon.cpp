//
// Created by chen on 2022/6/27.
//



#include "http_coon.h"

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

//设置文件非阻塞
void setNonBlock(int fd){
	int flag = fcntl(fd, F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(fd, F_SETFL, flag);
}

//向epoll添加fd 设置 事件触发   ET模式 设置非阻塞
void fdAdd(int ep, int fd, bool oneShot){
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	if(oneShot) event.events |= EPOLLONESHOT;
	epoll_ctl(ep, EPOLL_CTL_ADD, fd, &event);
	setNonBlock(fd);
}

//向epoll删除 fd
void fdRemove(int ep, int fd){
	epoll_ctl(ep, EPOLL_CTL_DEL, fd, nullptr);
	close(fd);
}
//向epoll 重新修改 fd事件  ET模式
void fdMode(int ep, int fd, int ev){
	epoll_event event;
	event.data.fd = fd;
	event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
	epoll_ctl(ep, EPOLL_CTL_MOD, fd, &event);
}

int http_coon::ep_fd = -1;
const char *http_coon::docRoot = "/var/www/server";

http_coon::http_coon() {
//	this->fileName = "/index.html";
//	this->version = "HTTP/1.1";
}

http_coon::~http_coon() {

}

void http_coon::init(int fd, const sockaddr_in & add) {
	this->sock_fd = fd;
	this->address = add;
	//将文件描述符 添加到epoll中 监听
	fdAdd(this->ep_fd, fd, true);
	init();
}

void http_coon::init() {
	//初始化 读相关
	memset(this->readBuff, 0 , sizeof this->readBuff);
	this->readIdx = 0;
	this->checkedIdx = 0;
	this->startIdx = 0;
	//初始化 写相关
	memset(this->writeBuff, 0, sizeof this->writeBuff);
	this->writeIdx = 0;
	this->canWrite = false;
	this->bytesToSend = 0;
	this->bytesHaveSend = 0;
	this->iovCount = 2;

	this->keepAlive = false;

	this->header.clear();
	this->post.clear();

	this->fileAddress = nullptr;

	//设置主状态机 初始状态
	this->checkState = CHECK_STATE_REQUEST_LINE;

	this->fileName = nullptr;
	this->version = nullptr;
	this->m_content = nullptr;

	//sql
	this->conn = nullptr;
}

void http_coon::close_fd() {
	if(this->sock_fd != -1){
		fdRemove(this->ep_fd, this->sock_fd);
	}
}

bool http_coon::read() {
	if(this->readIdx >= READ_BUFFER_SIZE) return false;
	while(true){
		//非阻塞 需要循环读取 同时 不断递增读入的下标
		int ret = recv(sock_fd, this->readBuff + this->readIdx, READ_BUFFER_SIZE - this->readIdx, 0);
		if(ret < 0){
			if(errno ==  EAGAIN || errno == EWOULDBLOCK){
				break;
			}
			return false;
		}else if(ret == 0){
			//断开连接 的情况 通过EPOLLRDHUB监听
			return false;
		}else{
			readIdx += ret;
		}
	}
//	std::cout << readBuff;
	return true;
}

bool http_coon::write() {
	if(!canWrite) return false;
	if(this->bytesToSend == 0){
		//改为监听 读事件  并且初始化连接
		init();
		fdMode(this->ep_fd, this->sock_fd, EPOLLIN);
		return true;
	}
	int ret;
	while(true){
		ret = writev(this->sock_fd, this->iov, this->iovCount);
		if(ret <= -1){
			if(errno == EAGAIN){
				//tcp发送缓冲区 暂时 没有空位 还有数据要发送 等待下一次 EPOLLOUT 事件
				fdMode(this->ep_fd, this->sock_fd, EPOLLOUT);
				return true;
			}

			unmap();
			return false;
		}

		bytesToSend -= ret;
		bytesHaveSend += ret;

		if(bytesHaveSend >= this->writeIdx){
			//iov[0]已近发送完毕
			this->iov[0].iov_len = 0;
			if(iovCount == 2){
				this->iov[1].iov_base = this->fileAddress + (bytesHaveSend - this->writeIdx);
				this->iov[1].iov_len = bytesToSend;
			}
		}
		else{
			//iov[0]还没有发送完毕
			this->iov[0].iov_base = this->writeBuff + bytesHaveSend;
			this->iov[0].iov_len = this->writeIdx - bytesHaveSend;
		}

		if(bytesToSend <= 0){
			//表示需要发送的数据 已经全部发送完毕
			unmap();
			if(this->keepAlive){
				//保持连接 改为监听 读事件  并且初始化连接
				init();
				fdMode(this->ep_fd, this->sock_fd, EPOLLIN);
				return true;
			}
			else{
				return false;
			}
		}
	}
}

http_coon::HTTP_CODE http_coon::httpParse() {
	LINE_STATUS lineStatus = LINE_OK;
	HTTP_CODE ret = NO_REQUEST;
	char *text = nullptr;
	while((this->checkState == CHECK_STATE_CONTENT && lineStatus == LINE_OK) || (lineStatus = lineParse()) == LINE_OK){
		text = getLine();
		this->startIdx = this->checkedIdx;
		switch (this->checkState) {
			case(CHECK_STATE_REQUEST_LINE):{
				//请求行解析
				ret = requestLineParse(text);
				if(ret == BAD_REQUEST) return BAD_REQUEST;
				break;
			}
			case(CHECK_STATE_HEADER):{
				//头部字段 解析
				ret = headersParse(text);
				if(ret == BAD_REQUEST) return BAD_REQUEST;
				else if(ret == GET_REQUEST){
					//已经获取到 完整请求
					return doRequest();
				}
				break;
			}
			case(CHECK_STATE_CONTENT):{
				//消息体 解析
				ret = contentParse(text);
				if(ret == GET_REQUEST){
					//获取到 完整请求
					return doRequest();
				}
				//返回NO_REQUEST 消息体没有读完
				lineStatus = LINE_OPEN;
				break;
			}
			default:{
				return INTERNAL_ERROR;
			}
		}
	}
	return NO_REQUEST;
}

http_coon::LINE_STATUS http_coon::lineParse() {
	char temp;
	for(;this->checkedIdx < this->readIdx; this->checkedIdx++){
		temp = readBuff[checkedIdx];
		if(temp == '\r'){
			if(this->checkedIdx + 1 == this->readIdx){
				return LINE_OPEN;
			}else if(readBuff[checkedIdx + 1] == '\n'){
				readBuff[checkedIdx++] = '\0';
				readBuff[checkedIdx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}else if(temp == '\n'){
			if(checkedIdx >= 1 && readBuff[checkedIdx - 1] == '\r'){
				readBuff[checkedIdx - 1] = '\0';
				readBuff[checkedIdx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
	}
	return LINE_OPEN;
}

http_coon::HTTP_CODE http_coon::requestLineParse(char *text) {
	char *m_url = strpbrk(text, " \t");         //查找text中第一个出现 str2中字符的位置
	if (!m_url)
	{
		return BAD_REQUEST;
	}
	m_url[0] = '\0';
	m_url++;
	char *str_method = text;
	//TODO 暂时只处理 GET 和 POST
	if (strcasecmp(str_method, "GET") == 0) method = GET;
	else if (strcasecmp(str_method, "POST") == 0) method = POST;
	else return BAD_REQUEST;

	m_url += strspn(m_url, " \t");      //查找str1中第一个没有出现 str2中字符的位置
	char *ver = strpbrk(m_url, " \t");

	if (!ver)
		return BAD_REQUEST;
	*ver++ = '\0';
	ver += strspn(ver, " \t");
	if (strcasecmp(ver, "HTTP/1.1") != 0) return BAD_REQUEST;
	this->version = ver;

	if (strncasecmp(m_url, "http://", 7) == 0){
		m_url += 7;
		m_url = strchr(m_url, '/');
	}

	if (strncasecmp(m_url, "https://", 8) == 0)
	{
		m_url += 8;
		m_url = strchr(m_url, '/');
	}

	if (!m_url || m_url[0] != '/') return BAD_REQUEST;

	//目标文件的绝对路径
	this->fileName = m_url;
	//当url为/时，显示首页
//	if (strlen(m_url) == 1) this->fileName.append("index.html");

	//主状态及更新
	checkState = CHECK_STATE_HEADER;
	return NO_REQUEST;
}

http_coon::HTTP_CODE http_coon::headersParse(char * text) {
	if(text[0] == '\0'){
		//解析到空行
		//如果有 消息体内容 则改变主状态机状态 去读取 获取content内容
		if(header.count("content-length") > 0 && atoi(header["content-length"].c_str()) > 0){
			this->checkState = CHECK_STATE_CONTENT;
			return NO_REQUEST;
		}
		return GET_REQUEST;
	}
	else{
		std::string s = text;
		int pos = s.find(':');
		if(pos == std::string::npos){
			//没找到
			return BAD_REQUEST;
		}else{
			//TODO 更加准确的解析
			std::string key = s.substr(0, pos);
			text += (pos + 1);
			pos = strspn(text, " \t");
			std::string val =  text + pos;
			std::transform(key.begin(), key.end(), key.begin(), tolower);
			std::transform(val.begin(), val.end(), val.begin(), tolower);
//			std::cout << key << ' ' << val << '\n';
			header.insert(std::pair<std::string, std::string>(key, val));
			if(header.count("connection") > 0 && header["connection"] == "keep-alive"){
				//HTTP长连接
				this->keepAlive = true;
			}
		}
	}
	return NO_REQUEST;
}

http_coon::HTTP_CODE http_coon::contentParse(char * text){
	int len = atoi(header["content-length"].c_str());
	if(len == 0) return BAD_REQUEST;
	if(this->readIdx >= len + this->checkedIdx){
		text[len] = '\0';
		m_content = text;
		//如果是post 将函数体中的参数解析出来
		//name=asda&passwd=123123
		if(this->method == POST && header.count("content-type") > 0){
			//TODO 暂时只处理 post请求中 消息体类型 为 application/x-www-form-urlencoded 的请求
			if(header["content-type"] != "application/x-www-form-urlencoded") return BAD_REQUEST;
			std::string content = text;
			int pos_s = 0, pos_e;
			int position;
			std::string s;
			while((pos_e = content.find('&', pos_s)) != std::string::npos){
				s = content.substr(pos_s, pos_e - pos_s);
				position = s.find('=');
				this->post.insert(std::pair<std::string, std::string>(s.substr(0,position), s.substr(position + 1, std::string::npos)));
				pos_s = pos_e + 1;
			}
			s = content.substr(pos_s, std::string::npos);
			position = s.find('=');
			this->post.insert(std::pair<std::string, std::string>(s.substr(0,position), s.substr(position + 1, std::string::npos)));
		}
		return GET_REQUEST;
	}
	return http_coon::NO_REQUEST;
}

http_coon::HTTP_CODE http_coon::doRequest() {
	if(method != GET && method != POST) return BAD_REQUEST;
	std::string realPath;
	realPath.append(this->docRoot);
	//确定响应的文件
	if(method == POST){
		if(strcmp(fileName, "/login.html") == 0){
			std::string user_name = post["name"];
			std::string passwd = post["passwd"];
			//数据库查询 检查登陆请求
			MYSQL_RES *resut = sqlQueryUser(user_name, passwd);
			if(!resut) return INTERNAL_ERROR;
			//有查询结果 说明存在该用户
			if(mysql_fetch_row(resut)){
				realPath.append("/index.html");
			}
			else{
				realPath.append("/loginFail.html");
			}
			mysql_free_result(resut);
		}
		else if(strcmp(fileName, "/register.html") == 0){
			std::string user_name = post["name"];
			std::string passwd = post["passwd"];
			//数据库查询 提交注册请求
			if(sqlInsertUser(user_name, passwd)){
				realPath.append("/registerSuccess.html");
			}
			else return INTERNAL_ERROR;
		}
	}
	else{
		realPath.append(this->fileName);
		if(strcmp(this->fileName, "/") == 0) realPath.append("index.html");
	}

	if(stat(realPath.c_str(),&this->fileStat) < 0){
		return NO_RESOURCE;
	}

	if(!(this->fileStat.st_mode & S_IROTH)){
		return FORBIDDEN_REQUEST;
	}
	if(S_ISDIR(this->fileStat.st_mode)){
		return BAD_REQUEST;
	}
	int fd = open(realPath.c_str(), O_RDONLY);
	this->fileAddress = (char *) mmap(nullptr, this->fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	return FILE_REQUEST;
}

char *http_coon::getLine() {
	return readBuff + startIdx;
}

void http_coon::unmap() {
	if(this->fileAddress){
		munmap(this->fileAddress, this->fileStat.st_size);
		fileAddress = nullptr;
	}
}

bool http_coon::addResponse(const char *format, ...) {
	if(this->writeIdx >= WRITE_BUFFER_SIZE) return false;
	va_list arg_list;
	va_start(arg_list, format);
	int len = vsnprintf(writeBuff + writeIdx, WRITE_BUFFER_SIZE - 1 - writeIdx, format, arg_list);
	if(len >= WRITE_BUFFER_SIZE - 1 - writeIdx) return false;
	writeIdx += len;
	va_end(arg_list);
	return true;
}

bool http_coon::addStatusLine(int status, const char * title) {
	return addResponse("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool http_coon::addHeaders(int len) {
	bool ret = true;
	ret &= addContentLength(len);
	ret &= andLinger();
	ret &= addBlankLine();
	return ret;
}

bool http_coon::addContentLength(int len) {
	return addResponse("Content-Length: %d\r\n", len);
}

bool http_coon::andLinger() {
	return addResponse("Connection: %s\r\n", (keepAlive == true) ? "keep-alive" : "close");
}

bool http_coon::addBlankLine() {
	return addResponse("%s",  "\r\n");
}

bool http_coon::addContent(const char * content) {
	return addResponse("%s", content);
}

bool http_coon::fillResponse(http_coon::HTTP_CODE ret) {
	switch (ret) {
		case(INTERNAL_ERROR):{
			addStatusLine(INTERNAL_ERROR, error_500_title);
			addHeaders(strlen(error_500_form));
			if(!addContent(error_500_form)){
				return false;
			}
			break;
		}
		case(BAD_REQUEST):{
			addStatusLine(BAD_REQUEST, error_400_title);
			addHeaders(strlen(error_400_form));
			if(!addContent(error_400_form)){
				return false;
			}
			break;
		}
		case(NO_RESOURCE):{
			addStatusLine(NO_RESOURCE, error_404_title);
			addHeaders(strlen(error_404_form));
			if(!addContent(error_404_form)){
				return false;
			}
			break;
		}
		case(FORBIDDEN_REQUEST):{
			addStatusLine(FORBIDDEN_REQUEST, error_403_title);
			addHeaders(strlen(error_403_form));
			if(!addContent(error_403_form)){
				return false;
			}
			break;
		}
		case(FILE_REQUEST):{
			addStatusLine(200, ok_200_title);
			if(fileStat.st_size != 0){
				addHeaders(fileStat.st_size);
//				std::cout << writeBuff;
				iov[0].iov_len = writeIdx;
				iov[0].iov_base = writeBuff;
				iov[1].iov_base = fileAddress;
				iov[1].iov_len = fileStat.st_size;
				this->bytesToSend = iov[0].iov_len + iov[1].iov_len;
				iovCount = 2;
				return true;
			}
			else{
				const char *ok_string = "<html><body></body></html>";
				addHeaders(strlen(ok_string));
				if(!addContent(ok_string)){
					return false;
				}
			}
		}
		default: return false;
	}
	iov[0].iov_base = writeBuff;
	iov[0].iov_len = writeIdx;
	bytesToSend = iov[0].iov_len;
	iovCount = 1;
	return true;
}

MYSQL_RES *http_coon::sqlQueryUser(std::string& name, std::string& passwd) {
	//先从连接池中取一个连接
	connection_pool *connPool = connection_pool::GetInstance();
	//自动获取 释放conn资源
	connectionRAII mysqlcon(&this->conn, connPool);
	//在user表中检索username，passwd数据，浏览器端输入
	char stat[128];
	memset(stat, 0, sizeof stat);
	sprintf(stat,"select * from user where username = '%s' and passwd = '%s';", name.c_str(), passwd.c_str());
	if (mysql_query(this->conn, stat))
	{
		perror(mysql_error(this->conn));
		return nullptr;
	}

	//从表中检索完整的结果集
	MYSQL_RES *result = mysql_store_result(this->conn);
	return result;
}

bool http_coon::sqlInsertUser(std::string& name, std::string& passwd) {
	//先从连接池中取一个连接
	connection_pool *connPool = connection_pool::GetInstance();
	//自动获取 释放conn资源
	connectionRAII mysqlcon(&this->conn, connPool);
	//在user表中插入username 和 passwd 数据
	char stat[128];
	memset(stat, 0, sizeof stat);
	sprintf(stat,"INSERT INTO user(username, passwd) VALUES('%s', '%s');", name.c_str(), passwd.c_str());
	if (mysql_query(this->conn, stat))
	{
		perror(mysql_error(this->conn));
		return false;
	}
	return true;
}
