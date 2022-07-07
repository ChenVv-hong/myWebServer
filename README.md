### myWebServer

Linux下轻量级Web服务器，用于巩固网络编程。

#### 技术栈

- 使用模拟proactor的事件处理模式
- 使用epoll进行多路IO复用，采用LT+ET的模式，实现并发
- 通过半同步半反应堆线程池的工作线程处理数据
- 使用状态机解析HTTP请求报文，支持GET和POST请求解析
- 使用升序链表定时器容器来处理定时功能，从而处理非活跃连接
- 实现MySql数据库连接池，通过访问数据库实现web端用户注册，登陆功能。

#### 项目结构

├── **http** 
│  ├── http_coon.cpp 
│  └── http_coon.h 
├── main.cpp 
├── README.md 
├── **resources** 
│  ├── favicon.ico 
│  ├── index.html 
│  ├── loginFail.html 
│  ├── login.html 
│  ├── picture.html 
│  ├── registerFail.html 
│  ├── register.html 
│  ├── registerSuccess.html 
│  ├── video.html 
│  ├── **video.mp4** 
│  └── **wallhaven-72lej9.png** 
├── **server** 
│  ├── server.cpp 
│  └── server.h 
├── **sql** 
│  ├── sqlConnectionPool.cpp 
│  └── sqlConnectionPool.h 
├── **threadpool** 
│  ├── thread_pool.cpp 
│  └── thread_pool.h 
└── **timer** 
   ├── timer.cpp 
   └── timer.h

#### 开发环境

- **OS**: Manjaro Linux x86_64 **Kernel**: 5.10.126-1-MANJARO
- GCC 12.1
- CMake 3.23.2
- MySql 8
- CLion

#### 使用运行

将resources目录下的资源文件全部移动到 /var/www/server目录下

然后进行编译运行，注意依赖库！