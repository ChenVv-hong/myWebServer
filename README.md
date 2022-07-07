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

├── **http** 连接对象文件<br/>
│  ├── http_coon.cpp<br/> 
│  └── http_coon.h <br/>
├── main.cpp <br/>
├── README.md <br/>
├── **resources** 资源文件<br/>
│  ├── favicon.ico <br/>
│  ├── index.html <br/>
│  ├── loginFail.html <br/>
│  ├── login.html <br/>
│  ├── picture.html <br/>
│  ├── registerFail.html <br/>
│  ├── register.html <br/>
│  ├── registerSuccess.html<br/> 
│  ├── video.html <br/>
│  ├── **video.mp4** <br/>
│  └── **wallhaven-72lej9.png** <br/>
├── **server** 服务器类<br/>
│  ├── server.cpp <br/>
│  └── server.h <br/>
├── **sql** 数据库连接池<br/>
│  ├── sqlConnectionPool.cpp <br/>
│  └── sqlConnectionPool.h <br/>
├── **threadpool** 线程池<br/>
│  ├── thread_pool.cpp <br/>
│  └── thread_pool.h <br/>
└── **timer** 定时器容器<br/>
   ├── timer.cpp <br/>
   └── timer.h<br/>

#### 开发环境

- **OS**: Manjaro Linux x86_64 **Kernel**: 5.10.126-1-MANJARO
- GCC 12.1
- CMake 3.23.2
- MySql 8
- CLion

#### 使用运行

将resources目录下的资源文件全部移动到 /var/www/server目录下

然后进行编译运行，注意依赖库！