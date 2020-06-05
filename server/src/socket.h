#ifndef SOCKET_H
#define SOCKET_H

// 宏定义声明
#define MAX_TRANSFER_BYTE 8192  // 最大传输字节
#define MAXLEN 8192			    // 字段最大长度
#define MAXNUM 10			    // 参数最大数量
#define DEFAULT_PORT 21		// 默认连接端口

#define S_SUCCESS        0      // 执行成功
#define S_ERROR_SOCKET  -1      // 套接字创建失败 errno
#define S_ERROR_BIND    -2      // 绑定失败 errno
#define S_ERROR_LISTEN  -3      // 监听失败 errno
#define S_ERROR_CONNECt -4      // 连接失败 errno
#define S_ERROR_ACCEPT  -5      // 接受失败 errno
#define S_ERROR_READ    -6      // 读取失败 errno
#define S_ERROR_WRITE   -7      // 写入失败 errno
#define S_ERROR_FILE    -8      // 文件操作失败
#define S_CONNECT_BREAK -9      // 连接断开


/*
printf("Error socket(): %s(%d)\n", strerror(errno), errno);
printf("Error bind(): %s(%d)\n", strerror(errno), errno);
printf("Error listen(): %s(%d)\n", strerror(errno), errno);
printf("Error connect(): %s(%d)\n", strerror(errno), errno);
printf("Error accept(): %s(%d)\n", strerror(errno), errno);
*/

// 函数声明
int createListenSocket(unsigned int IP, unsigned int port);		                        // 创建套接字/初始化/进行监听（listen）
int createConnectionSocket(unsigned int IP, unsigned int port);		                    // 创建套接字/初始化/进行连接（connect）
int acceptConnection(int listenfd);					                                    // 等待连接（accept）
int readSocket(int connfd, char *sentence);				                                // 读取套接字（read）
int writeSocket(int connfd, char *sentence);				                            // 写入套接字（write）
int analyseCommand(char *sentence, char *command, char **commandArgs, int *commandArgsNumber);      // 命令行解析
int getResponseCode(char *sentence);                                                                // 响应代码获取
int analyseIPandPort(char *str, unsigned int *IP, unsigned int *port);	                            // IP/port 解析
int formatIPandPort(char *str, unsigned int IP, unsigned int port);	                                // IP/port 格式化
int getServerIP(char *strIP);								           // 获取本机 IP

#endif
