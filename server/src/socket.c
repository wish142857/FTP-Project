#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include <unistd.h>
#include <errno.h>

#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "socket.h"

/******************
注意：以下函数需要如下参数
	char sentence[MAXLEN];		// 字段
	char command[MAXLEN];		// 命令行
	char* commandArgs[MAXNUM];	// 命令行参数
	int commandArgsNumber = 0;	// 命令行参数数量
******************/


/******************
函数：创建套接字/初始化/进行监听（listen）
参数：IP 地址，port 号码
返回：listenfd 监听套接字 >= 0
错误代码 < 0  S_ERROR_SOCKET|S_ERROR_BIND|S_ERROR_LISTEN
******************/
int createListenSocket(unsigned int IP, unsigned int port) {
	// 创建 socket
	int listenfd;
	if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
		return S_ERROR_SOCKET;
	// 设置 ip 和 port
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;                   // Internet地址族
	addr.sin_port = htons(port);                 // 端口号
	addr.sin_addr.s_addr = htonl(IP);    	     // IP地址，监听"0.0.0.0"
	// 将 ip 和 port 与 socket 绑定
	if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
		return S_ERROR_BIND;
	// 开始监听 socket
	if (listen(listenfd, 10) == -1)
		return S_ERROR_LISTEN;
	return listenfd;
}


/******************
函数：创建套接字/初始化/进行连接（connect）
参数：IP 地址，port 号码
返回：sockfd 套接字 >= 0
错误代码 < 0  S_ERROR_SOCKET|S_ERROR_CONNECt
******************/
int createConnectionSocket(unsigned int IP, unsigned int port) {
	// 创建 socket
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
		return S_ERROR_SOCKET;
	// 设置目标主机的 ip 和 port
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;							// Internet地址族
	addr.sin_port = htons(port);						// 端口号
	memcpy(&addr.sin_addr, &IP, sizeof(IP));			// IP 地址
	// 将 socket 和目标主机连接 -- 阻塞函数
	if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
		return S_ERROR_CONNECt;
	return sockfd;
}


/******************
函数：等待连接（accept）
参数：listenfd 监听套接字
返回：connfd 连接套接字 >= 0
错误代码 < 0  S_ERROR_ACCEPT
******************/
int acceptConnection(int listenfd) {
	// 等待 client 连接 -- 阻塞函数
	printf("Waiting for client connection...\n");
	struct sockaddr_in addr;
	int connfd, addrlen = sizeof(addr);																																
	if ((connfd = accept(listenfd, (struct sockaddr *)&addr, &addrlen)) == -1)
		return S_ERROR_ACCEPT;
	printf("Successful connection to: %s\n", inet_ntoa(addr.sin_addr));
	return connfd;
}

/******************
函数：读取套接字（read）
******************/
int readSocket(int connfd, char *sentence) {
	int p = 0;
	// 榨干 socket 传来的内容
	do {
		int n = read(connfd, sentence + p, MAXLEN - p);
		if (n < 0)
			return S_ERROR_READ;
		if (n == 0)
			return S_CONNECT_BREAK;
		p += n;

	} while (sentence[p - 1] != '\n');
	// socket接收到的字符串并不会添加'\0'
	if ((p > 1) && (sentence[p - 1] == '\n'))
		sentence[--p] = '\0';
	if ((p > 1) && (sentence[p - 1] == '\r'))
		sentence[--p] = '\0';	
	return S_SUCCESS;
}

/******************
函数：写入套接字（write）
******************/
int writeSocket(int connfd, char *sentence) {
	// 字段处理
	int p = 0, sentenceLength = strlen(sentence);
	sentence[sentenceLength++] = '\r';
	sentence[sentenceLength++] = '\n';
	sentence[sentenceLength] = '\0';
	// 发送字符串到 socket
	while (p < sentenceLength) {
		int n = write(connfd, sentence + p, sentenceLength - p);
		if (n < 0) {
			return S_ERROR_WRITE;
		}
		p += n;
	}
	// 字段逆处理
	sentence[--sentenceLength] = '\0';
	sentence[--sentenceLength] = '\0';
	return S_SUCCESS;
}


/******************
函数：命令行解析
参数：command 命令字符串
返回：解析成功返回 0，解析失败返回 -1
******************/
int analyseCommand(char *sentence, char *command, char **commandArgs, int *commandArgsNumber) {
	// 命令行参数获取
	strcpy(command, sentence);
	*commandArgsNumber = 0;
	int i = 0, j = 0, length = strlen(command);
	char delimiter = ' ';
	while (i < length) {
		while (command[i] == delimiter)
			if(++i >= length) break;
		if (i >= length) break;
		j = i + 1;
		while (command[j] != delimiter)
			if(++j >= length) break;
		if (*commandArgsNumber >= MAXNUM)
			return -1;
		commandArgs[*commandArgsNumber] = &command[i];
		*commandArgsNumber += 1;
		command[j] = '\0';
		i = j + 1;
	}
	if (*commandArgsNumber <= 0)
		return -1;
	return 0;
}


/******************
函数：响应代码获取
参数：sentence 响应报文
返回：获取成功返回代码 > 0
获取失败返回 <= 0
******************/
int getResponseCode(char *sentence) {	
	if (strlen(sentence) < 3) return -1;
	char code[4];
	code[0] = sentence[0];
	code[1] = sentence[1];
	code[2] = sentence[2];
	code[3] = '\0';
	return atoi(code);
}



/******************
函数：IP/port 解析
功能：从 h1,h2,h3,h4,p1,p2 格式中解析 IP 与 port
返回：解析成功返回 0  解析失败返回 -1
******************/
int analyseIPandPort(char *str, unsigned int *IP, unsigned int *port) {
	// 参数检验
	int length = strlen(str);
	if ((length < 11) || (length > 23)) return -1;
	char cpy[32];
	strcpy(cpy, str);
	for (int i = 0; i < length; i++)
		if ((str[i] !=',') && ((str[i] > '9') || (str[i] < '0'))) return -1;
  	// IP 提取
  	unsigned int tempIP = 0, tempPort = 0, tempNum = 0;
	char tempStr[32];
  	int t= 0, i = -1, j = 0;
  	for (t = 0; t < 4; t++) {
  		while(++i < length)
  			if (str[i] == ',') {
  				cpy[i] = '.';
  				break;
  			}
  		if (i >= length) return -1;
  	}
  	memset(tempStr, 0, sizeof(tempStr));
  	strncpy(tempStr, cpy, i);
  	tempIP = inet_addr(tempStr);
  	// port 提取
  	j = i++;  	
  	while(++j < length)
  		if (str[j] == ',') break;
  	if ((j >= length) || ((j - i) >= 4) || ((j - i) <= 0)) return -1;
  	memset(tempStr, 0, sizeof(tempStr));
  	strncpy(tempStr, &str[i], j - i);
  	tempNum = atoi(tempStr);
  	tempPort = tempNum;
  	i = ++j;
  	j = length;
  	if (((j - i) >= 4) || ((j - i) <= 0)) return -1;
  	memset(tempStr, 0, sizeof(tempStr));
  	strncpy(tempStr, &str[i], j - i);
  	tempNum = atoi(tempStr);
  	tempPort *= 256;
  	tempPort += tempNum;
  	*IP = tempIP;
  	*port = tempPort;
  	return 0;
}

/******************
函数：IP/port 格式化
功能：将 IP 与 port 格式化为 h1,h2,h3,h4,p1,p2
返回：格式化成功返回 0  格式化失败返回 -1
******************/
int formatIPandPort(char *str, unsigned int IP, unsigned int port) {
	memset(str, 0, sizeof(str));	
	int h1 = IP % 256; IP /= 256;
	int h2 = IP % 256; IP /= 256;
	int h3 = IP % 256; IP /= 256;
	int h4 = IP % 256;
	int p2 = port % 256; port /= 256;
	int p1 = port % 256;
	sprintf(str, "%d,%d,%d,%d,%d,%d", h1, h2, h3, h4, p1, p2);
	return 0;
}


/******************
函数：获取本机 IP
返回：成功返回 0  失败返回 -1
******************/
int getServerIP(char *strIP) {
	int i = 0;
	int sockfd;
	struct ifconf ifc;
	char buf[1024] = {0};
	char ipbuf[32] = {0};
	struct ifreq *ifr;
	ifc.ifc_len = 1024;
	ifc.ifc_buf = buf;
	if((sockfd = socket(AF_INET, SOCK_DGRAM,0)) < 0)
		return -1;
	ioctl(sockfd, SIOCGIFCONF, &ifc);
	ifr = (struct ifreq*)buf;
	for(i = (ifc.ifc_len / sizeof(struct ifreq)); i > 0; i--)
	{
		inet_ntop(AF_INET,&((struct sockaddr_in *)&ifr->ifr_addr)->sin_addr,ipbuf,32);
		if (strcmp(ipbuf, "127.0.0.1") != 0) {
			strcpy(strIP, ipbuf);
			return 0;
		}
		ifr = ifr + 1;
	}
	return -1;
}




