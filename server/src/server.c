#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>


#include "socket.h"
#include "filer.h"



unsigned int serverIP = 0, serverPort = 0;  				// 服务端 IP/port
char serverRootPath[MAX_ROOT_PATH_LENGTH];					// 服务端根目录路径


/******************
函数：参数数量检查
返回：0: 数量合法  -1: 数量非法
******************/
int checkArgsNum(char *sentence, char *command, int commandArgsNumber, int minNum, int maxNum) {
	if (commandArgsNumber - 1 > maxNum) {
		sprintf(sentence, "501 Error args: too many args for '%s'.", command);
		return -1;
	}
	if (commandArgsNumber - 1 < minNum) {
		sprintf(sentence, "501 Error args: too few args for '%s'.", command);
		return -1;
	}
	return 0;
}

/******************
函数：USER 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 sentence
******************/
int USER(char *argument, char *sentence, int *waitingForUSER, int *waitingForPASS) {
	if (strcmp(argument, "anonymous") != 0) {
		strcpy(sentence, "504 Invalid args: expect 'USER anonymous'.");
		return -1;
	}
	*waitingForUSER = 0;
	*waitingForPASS = 1;
	strcpy(sentence, "331 Guest login OK: please send your complete e-mail address as password.");
	return 0;
}

/******************
函数：PASS 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 sentence
******************/
int PASS(char *argument, char *sentence, int *waitingForUSER, int *waitingForPASS, int *isLogin) {
	// 用户保存 TODO
	if (*waitingForPASS == 0) {
		strcpy(sentence, "332 Expect 'USER' first.");
		return -1;
	}
	*waitingForUSER = 0;
	*waitingForPASS = 0;
	*isLogin = 1;
	strcpy(sentence, "230 Guest login OK: access restrictions apply, WELCOME!");
	return 0;
}

/******************
函数：PORT 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 sentence
******************/
int PORT(char *argument, char *sentence, unsigned int *connIP, unsigned int *connPort, int *isPort, int *isPasv) {
	// 参数格式检测
	if (analyseIPandPort(argument, connIP, connPort) < 0) {
		strcpy(sentence, "501 Error args: wrong format.");
		return -1;
	}
	// 切换到主动模式
	*isPort = 1;
	*isPasv = 0;
	strcpy(sentence, "200 PORT command successful.");
	return 0;							
}

/******************
函数：PASV 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 sentence
******************/
int PASV(char *sentence, int *listenfd, unsigned int *connIP, unsigned int *connPort, int *isPort, int *isPasv) {
	// 切换到被动模式
	*isPort = 0;
	*isPasv = 1;
	// 创建监听套接字
	do {
		*connIP = serverIP;						// 服务端 IP
		*connPort = rand() % (45536) + 20000;	// 生成 20000~65535 之间随机 port
	} while ((*listenfd = createListenSocket(INADDR_ANY, *connPort)) < 0);
	char addr[32];
	formatIPandPort(addr, *connIP, *connPort);
	sprintf(sentence, "227 Entering Passive Mode (%s)", addr);
	return 0;
}

/******************
函数：RETR 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 sentence
******************/
int RETR(char *argument, char *filename, char *sentence, int *connfd, int *listenfd, 
	unsigned int *connIP, unsigned int *connPort, int *isPort, int *isPasv, int *fileShift) {
	if (!(*isPort) && !(*isPasv)) {
		strcpy(sentence, "500 Invalid command: expect 'PORT' or 'PASV' before.");
		return -1;
	}
	// 判断存在性 & 类型
    if (access(argument, F_OK) != 0) {
		strcpy(sentence, "450 File operation failed: invalid path.");
		*isPort = *isPasv = 0;
		return -1;
	}
	struct stat s_stat;                     // 文件路径信息
    stat(argument, &s_stat);            	// 获取路径信息
    if (!S_ISREG(s_stat.st_mode)) {
		strcpy(sentence, "450 File operation failed: non file.");
		*isPort = *isPasv = 0;
		return -1;
	}
    long size = s_stat.st_size;
	// 打开文件
	int file = open(argument, O_RDONLY);
	if (file < 0) {
		strcpy(sentence, "451 Unable to open file.");
		*isPort = *isPasv = 0;
		return -1;
	}	
	// 创建套接字
	int datafd = -1;
	if (*isPort) {
		if ((datafd = createConnectionSocket(*connIP, *connPort)) < 0) {			// 主动模式 进行连接
			close(file);
			strcpy(sentence, "425 Unable to open data connection.");
			*isPort = *isPasv = 0;
			return -1;
		}
	} else if (*isPasv) {
		if ((datafd = acceptConnection(*listenfd)) < 0) {
			close(file);
			strcpy(sentence, "425 Unable to open data connection.");
			*isPort = *isPasv = 0;
			return -1;
		}
	}
	*isPort = *isPasv = 0;
	// 传输文件
	sprintf(sentence, "150 Opening BINARY mode data connection for %s (%ld bytes)", filename, size);
	if (writeSocket(*connfd, sentence) < 0) {
		close(file);
		close(datafd);
		strcpy(sentence, "426 Data connection interrupted.");
		return -1;
	}
	// 文件设置偏移量
	lseek(file, *fileShift, SEEK_SET);
	int n = 0, m = 0, p = 0;
	char buf[MAX_TRANSFER_BYTE + 10];
	while((n = read(file, buf, MAX_TRANSFER_BYTE)) > 0) {
		p = 0;
		while (p < n) {
			m = write(datafd, buf, n);
			if (m < 0) {
				close(file);
				close(datafd);
				strcpy(sentence, "426 Data connection interrupted.");
				return -1;
			}
			p += m;
		}
	}
	close(file);
	close(datafd);
	strcpy(sentence, "226 Transfer complete.");
	return 0;
}


/******************
函数：STOR 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 sentence
******************/
int STOR(char *argument, char *filename, char *sentence, int *connfd, int *listenfd, 
	unsigned int *connIP, unsigned int *connPort, int *isPort, int *isPasv) {
	if (!(*isPort) && !(*isPasv)) {
		strcpy(sentence, "500 Invalid command: expect 'PORT' or 'PASV' before.");
		return -1;
	}
	// 打开文件
	int file = open(argument, O_WRONLY | O_CREAT | O_TRUNC, 0664);
	if (file < 0) {
		strcpy(sentence, "451 Unable to create file.");
		*isPort = *isPasv = 0;
		return -1;
	}
	// 获取大小
	struct stat s_stat;                     // 文件路径信息
    stat(argument, &s_stat);                // 获取路径信息
	long size = s_stat.st_size;
	// 创建套接字
	int datafd = -1;
	if (*isPort) {
		if ((datafd = createConnectionSocket(*connIP, *connPort)) < 0) {			// 主动模式 进行连接
			close(file);
			strcpy(sentence, "425 Unable to open data connection.");
			*isPort = *isPasv = 0;
			return -1;
		}
	} else if (*isPasv) {
		if ((datafd = acceptConnection(*listenfd)) < 0) {
			close(file);
			strcpy(sentence, "425 Unable to open data connection.");
			*isPort = *isPasv = 0;
			return -1;
		}
	}
	*isPort = *isPasv = 0;
	// 传输文件
	sprintf(sentence, "150 Opening BINARY mode data connection for %s (%ld bytes)", filename, size);
	if (writeSocket(*connfd, sentence) < 0) {
		close(file);
		close(datafd);
		strcpy(sentence, "426 Data connection interrupted.");
		return -1;
	}
	int n = 0;
	char buf[MAX_TRANSFER_BYTE + 10];
	while ((n = read(datafd, buf, MAX_TRANSFER_BYTE)) > 0) {
		if (n <= 0)
			break;
		if (write(file, buf, n) < 0) {
			close(file);
			close(datafd);
			strcpy(sentence, "426 Data connection interrupted.");
			return -1;
		}
	}
	close(file);
	close(datafd);
	strcpy(sentence, "226 Transfer complete.");
	return 0;
}



/******************
函数：SYST 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 sentence
******************/
int SYST(char *sentence) {
	strcpy(sentence, "215 UNIX Type: L8");
	return 0;
}




/******************
函数：TYPE 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 sentence
******************/
int TYPE(char *argument, char *sentence) {
	if (strcmp(argument, "I") != 0) {
		strcpy(sentence, "504 Invalid args: expect 'TYPE I'.");
		return -1;
	}
	strcpy(sentence, "200 Type set to I.");
	return 0;
}

/******************
函数：LIST 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 sentence
******************/
int LIST(char *argument, char *sentence, char *rootPath, char *workPath,
		int *connfd, int *listenfd, unsigned int *connIP, unsigned int *connPort, int *isPort, int *isPasv) {
	if (!(*isPort) && !(*isPasv)) {
		strcpy(sentence, "500 Invalid command: expect 'PORT' or 'PASV' before.");
		return -1;
	}
	// 调用 listDirectory
	char directory[MAXLEN * 2];
	int ret = listDirectory(rootPath, workPath, argument, directory);
	// 错误处理
	switch (ret) {
		case F_SUCCESS:
			break;
		case F_ERROR:
			sprintf(sentence, "450 File operation failed: %s(%d)", strerror(errno), errno);
			*isPort = *isPasv = 0;
			return -1;
		case F_NOTFOUND:
			strcpy(sentence, "450 File operation failed: invalid path.");
			*isPort = *isPasv = 0;
			return -1;
		default:
			break;
	}
	// 创建套接字
	int datafd = -1;
	if (*isPort) {
		if ((datafd = createConnectionSocket(*connIP, *connPort)) < 0) {			// 主动模式 进行连接
			strcpy(sentence, "425 Unable to open data connection.");
			*isPort = *isPasv = 0;
			return -1;
		}
	} else if (*isPasv) {
		if ((datafd = acceptConnection(*listenfd)) < 0) {
			strcpy(sentence, "425 Unable to open data connection.");
			*isPort = *isPasv = 0;
			return -1;
		}
	}
	*isPort = *isPasv = 0;
	// 传输目录
	strcpy(sentence, "150 Sending directory list.");
	if (writeSocket(*connfd, sentence) < 0) {
		close(datafd);
		strcpy(sentence, "426 Data connection interrupted.");
		return -1;
	}
	if (writeSocket(datafd, directory) < 0) {
		close(datafd);
		strcpy(sentence, "426 Data connection interrupted.");
		return -1;
	}
	close(datafd);
	strcpy(sentence, "226 Transfer complete.");
	return 0;
}




/******************
函数：QUIT/ABOR 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 sentence
******************/
// 实现不在此处

/******************
函数：MKD 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 sentence
******************/
// 实现不在此处

/******************
函数：CWD 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 sentence
******************/
// 实现不在此处

/******************
函数：PWD 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 sentence
******************/
// 实现不在此处

/******************
函数：RMD 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 sentence
******************/
// 实现不在此处

/******************
函数：RNFR 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 sentence
******************/
// 实现不在此处

/******************
函数：RNTO 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 sentence
******************/
// 实现不在此处

/******************
函数：REST 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 sentence
******************/
int REST(char *argument, char *sentence, int *fileShift) {
	*fileShift = atol(argument);
	sprintf(sentence, "350 Restarting at %d.", *fileShift);
	return 0;
}



/******************
函数：服务端连接交互 多线程
******************/
void* handleConnection(void* argv) {
	// *** 变量定义 ***
	int connfd = *(int*)argv;					// 连接套接字（指令流）
	int listenfd = -1;							// 监听套接字（数据流）
	unsigned int connIP = 0, connPort = 0;		// 连接 IP/port
	int isLogin = 0, isPort = 0, isPasv = 0;	// 状态标识
	int waitingForUSER = 0, waitingForPASS = 0;	// 状态标识

	char sentence[MAXLEN];						// 字段
	char command[MAXLEN];						// 命令行
	char* commandArgs[MAXNUM];					// 命令行参数
	int commandArgsNumber = 0;					// 命令行参数数量
    char rootPath[MAX_ROOT_PATH_LENGTH];    	// 根路径
    char workPath[MAX_WORK_PATH_LENGTH];    	// 工作路径
    char targetPath[MAX_WORK_PATH_LENGTH];  	// 目标路径
    char renamePath[MAX_WORK_PATH_LENGTH];  	// 重命名路径
    int isRenaming = 0;                     	// 是否处于重命名状态
	int fileShift = 0;							// 文件偏移

	// *** 变量初始化 ***
	memset(sentence, 0, sizeof(sentence));
	memset(command, 0, sizeof(command));
	strcpy(rootPath, serverRootPath);
	memset(workPath, 0, sizeof(workPath));
	memset(targetPath, 0, sizeof(targetPath));
	memset(renamePath, 0, sizeof(renamePath));
	// *** 设置初始消息 ***
	strcpy(sentence, "220 Anonymous FTP server ready.");
	switch (writeSocket(connfd, sentence)) {
		case S_SUCCESS:
			break;
		case S_ERROR_WRITE: 
			printf("Error write(): %s(%d)\nConnection closed.\n", strerror(errno), errno);
			if (connfd >= 0) close(connfd);
			if (listenfd >= 0) close(listenfd);
			return NULL;
	}
	waitingForUSER = 1;
	// *** 持续连接交互 ***
	while(1) {
		// ** 读取套接字 **
		switch (readSocket(connfd, sentence)) {
			case S_SUCCESS:
				break;
			case S_ERROR_READ: 
				printf("Error read(): %s(%d)\nConnection closed.\n", strerror(errno), errno);
				if (connfd >= 0) close(connfd);
				if (listenfd >= 0) close(listenfd);
				return NULL;
			case S_CONNECT_BREAK:
				printf("Connection closed.\n");
				if (connfd >= 0) close(connfd);
				if (listenfd >= 0) close(listenfd);
				return NULL;
		}
		// ** 命令解析&处理 **
		if (analyseCommand(sentence, command, commandArgs, &commandArgsNumber) == 0) {
			if ((waitingForUSER == 1) && (strcmp(commandArgs[0], "USER") != 0))  {
				strcpy(sentence, "500 Invalid command: expect 'USER'.");
			} else if ((waitingForPASS == 1) && (strcmp(commandArgs[0], "PASS") != 0))  {
				strcpy(sentence, "500 Invalid command: expect 'PASS'.");
			} else {
				// ---[USER]---  331/504			
				if (strcmp(commandArgs[0], "USER") == 0) {
					if (checkArgsNum(sentence, commandArgs[0], commandArgsNumber, 1, 1) == 0) {
						USER(commandArgs[1], sentence, &waitingForUSER, &waitingForPASS);
					}
				}
				// ---[PASS]---  230/332
				else if (strcmp(commandArgs[0], "PASS") == 0) {
					if (checkArgsNum(sentence, commandArgs[0], commandArgsNumber, 0, 1) == 0) {
						if (commandArgsNumber == 1)
							PASS(NULL, sentence, &waitingForUSER, &waitingForPASS, &isLogin);
						else
							PASS(commandArgs[1], sentence, &waitingForUSER, &waitingForPASS, &isLogin);
					}
				}
				// ---[PORT]---  200/501
				else if (strcmp(commandArgs[0], "PORT") == 0) {
					if (checkArgsNum(sentence, commandArgs[0], commandArgsNumber, 1, 1) == 0) {
						// 关闭现有套接字
						if (listenfd >= 0) {
							close(listenfd);
							listenfd = -1;
						}
						PORT(commandArgs[1], sentence, &connIP, &connPort, &isPort, &isPasv);
					}
				}
				// ---[PASV]---  227
				else if (strcmp(commandArgs[0], "PASV") == 0) {
					if (checkArgsNum(sentence, commandArgs[0], commandArgsNumber, 0, 0) == 0) {
						// 关闭现有套接字
						if (listenfd >= 0) {
							close(listenfd);
							listenfd = -1;
						}
						PASV(sentence, &listenfd, &connIP, &connPort, &isPort, &isPasv);
					}
				}
				// ---[RETR]---
				else if (strcmp(commandArgs[0], "RETR") == 0) {
					if (checkArgsNum(sentence, commandArgs[0], commandArgsNumber, 1, 1) == 0) {
						char completePath[MAX_ROOT_PATH_LENGTH + MAX_WORK_PATH_LENGTH];
						getCompletePath(rootPath, workPath, commandArgs[1], completePath);
						RETR(completePath, commandArgs[1], sentence, &connfd, &listenfd, &connIP, &connPort, &isPort, &isPasv, &fileShift);
						isPort = isPasv = 0;
						fileShift = 0;
					}
				}
				// ---[STOR]---
				else if (strcmp(commandArgs[0], "STOR") == 0) {
					if (checkArgsNum(sentence, commandArgs[0], commandArgsNumber, 1, 1) == 0) {
						char completePath[MAX_ROOT_PATH_LENGTH + MAX_WORK_PATH_LENGTH];
						getCompletePath(rootPath, workPath, commandArgs[1], completePath);
						STOR(completePath, commandArgs[1], sentence, &connfd, &listenfd, &connIP, &connPort, &isPort, &isPasv);
						isPort = isPasv = 0;
					}
				}
				// ---[SYST]---
				else if (strcmp(commandArgs[0], "SYST") == 0) {
					if (checkArgsNum(sentence, commandArgs[0], commandArgsNumber, 0, 0) == 0) {
						SYST(sentence);
					}
				}
				// ---[TYPE]---	
				else if (strcmp(commandArgs[0], "TYPE") == 0) {
					if (checkArgsNum(sentence, commandArgs[0], commandArgsNumber, 1, 1) == 0) {
						TYPE(commandArgs[1], sentence);
					}
				}
				// ---[QUIT/ABOR]---
				else if ((strcmp(commandArgs[0], "QUIT") == 0) || ((strcmp(commandArgs[0], "ABOR") == 0))) {
					if (checkArgsNum(sentence, commandArgs[0], commandArgsNumber, 0, 0) == 0) {
						// 发送结束信息
						strcpy(sentence, "221 Service is about to shut down.");
						writeSocket(connfd, sentence);
						// 关闭所有套接字并退出
						if (connfd >= 0) close(connfd);
						if (listenfd >= 0) close(listenfd);
						return NULL;
					}
				}
				// ---[LIST]---
				else if (strcmp(commandArgs[0], "LIST") == 0) {
					if (checkArgsNum(sentence, commandArgs[0], commandArgsNumber, 0, 1) == 0) {
						if (commandArgsNumber == 1) {
							// 无参数
							LIST(NULL, sentence, rootPath, workPath, &connfd, &listenfd, &connIP, &connPort, &isPort, &isPasv);
						} else {
							// 有参数
							strcpy(targetPath, commandArgs[1]);
							LIST(targetPath, sentence, rootPath, workPath, &connfd, &listenfd, &connIP, &connPort, &isPort, &isPasv);
						}
					}
				}
				// ---[MKD]---  250/450
				else if (strcmp(commandArgs[0], "MKD") == 0) {
					if (checkArgsNum(sentence, commandArgs[0], commandArgsNumber, 1, 1) == 0) {							
						strcpy(targetPath, commandArgs[1]);
						int  ret = makeDirectory(rootPath, workPath, targetPath);
						// 错误处理
						switch (ret) {
							case F_SUCCESS:
								strcpy(sentence, "257 File operation succeeded.");
								break;
							case F_ERROR:
								sprintf(sentence, "450 File operation failed: %s(%d)", strerror(errno), errno);
								break;
							case F_OVERFLOW:
								strcpy(sentence, "450 File operation failed: too long path.");
								break;
							case F_NOTFOUND:
								strcpy(sentence, "450 File operation failed: invalid path.");
								break;
							case F_NORENAME:
								strcpy(sentence, "450 File operation failed: expect 'RNFR' first.");
								break;
							case F_ISROOT:
								strcpy(sentence, "450 File operation failed: already at root.");
								break;
							default:
								strcpy(sentence, "450 File operation failed.");
								break;
						}
					}
				}
				// ---[CWD]---  250/450
				else if (strcmp(commandArgs[0], "CWD") == 0) {
					if (checkArgsNum(sentence, commandArgs[0], commandArgsNumber, 1, 1) == 0) {
						strcpy(targetPath, commandArgs[1]);
						int ret = changeWorkDirectory(rootPath, workPath, targetPath);
						// 错误处理
						switch (ret) {
							case F_SUCCESS:
								strcpy(sentence, "250 File operation succeeded.");
								break;
							case F_ERROR:
								sprintf(sentence, "450 File operation failed: %s(%d)", strerror(errno), errno);
								break;
							case F_OVERFLOW:
								strcpy(sentence, "450 File operation failed: too long path.");
								break;
							case F_NOTFOUND:
								strcpy(sentence, "450 File operation failed: invalid path.");
								break;
							case F_NORENAME:
								strcpy(sentence, "450 File operation failed: expect 'RNFR' first.");
								break;
							case F_ISROOT:
								strcpy(sentence, "450 File operation failed: already at root.");
								break;
							default:
								strcpy(sentence, "450 File operation failed.");
								break;
						}
					}
				}
				// ---[RMD]---  250/450
				else if (strcmp(commandArgs[0], "RMD") == 0) {
					if (checkArgsNum(sentence, commandArgs[0], commandArgsNumber, 1, 1) == 0) {
						strcpy(targetPath, commandArgs[1]);
						int ret = removeDirectory(rootPath, workPath, targetPath);
						// 错误处理
						switch (ret) {
							case F_SUCCESS:
								strcpy(sentence, "250 File operation succeeded.");
								break;
							case F_ERROR:
								sprintf(sentence, "450 File operation failed: %s(%d)", strerror(errno), errno);
								break;
							case F_OVERFLOW:
								strcpy(sentence, "450 File operation failed: too long path.");
								break;
							case F_NOTFOUND:
								strcpy(sentence, "450 File operation failed: invalid path.");
								break;
							case F_NORENAME:
								strcpy(sentence, "450 File operation failed: expect 'RNFR' first.");
								break;
							case F_ISROOT:
								strcpy(sentence, "450 File operation failed: already at root.");
								break;
							default:
								strcpy(sentence, "450 File operation failed.");
								break;
						}
					}
				}
				// ---[PWD]---  250/450
				else if (strcmp(commandArgs[0], "PWD") == 0) {
					if (checkArgsNum(sentence, commandArgs[0], commandArgsNumber, 0, 0) == 0) {
						int ret = printWorkDirectory(rootPath, workPath, sentence);
					}
				}
				// ---[RNFR]---  250/450
				else if (strcmp(commandArgs[0], "RNFR") == 0) {
					if (checkArgsNum(sentence, commandArgs[0], commandArgsNumber, 1, 1) == 0) {
						strcpy(targetPath, commandArgs[1]);
						int ret = renameFR(rootPath, workPath, targetPath, renamePath, &isRenaming);
						// 错误处理
						switch (ret) {
							case F_SUCCESS:
								strcpy(sentence, "350 File operation succeeded.");
								break;
							case F_ERROR:
								sprintf(sentence, "450 File operation failed: %s(%d)", strerror(errno), errno);
								break;
							case F_OVERFLOW:
								strcpy(sentence, "450 File operation failed: too long path.");
								break;
							case F_NOTFOUND:
								strcpy(sentence, "450 File operation failed: invalid path.");
								break;
							case F_NORENAME:
								strcpy(sentence, "450 File operation failed: expect 'RNFR' first.");
								break;
							case F_ISROOT:
								strcpy(sentence, "450 File operation failed: already at root.");
								break;
							default:
								strcpy(sentence, "450 File operation failed.");
								break;
						}
					}	
				}
				// ---[RNTO]---  250/450
				else if (strcmp(commandArgs[0], "RNTO") == 0) {
					if (checkArgsNum(sentence, commandArgs[0], commandArgsNumber, 1, 1) == 0) {
						strcpy(targetPath, commandArgs[1]);
						int ret = renameTO(rootPath, workPath, targetPath, renamePath, &isRenaming);
						// 错误处理
						switch (ret) {
							case F_SUCCESS:
								strcpy(sentence, "250 File operation succeeded.");
								break;
							case F_ERROR:
								sprintf(sentence, "450 File operation failed: %s(%d)", strerror(errno), errno);
								break;
							case F_OVERFLOW:
								strcpy(sentence, "450 File operation failed: too long path.");
								break;
							case F_NOTFOUND:
								strcpy(sentence, "450 File operation failed: invalid path.");
								break;
							case F_NORENAME:
								strcpy(sentence, "450 File operation failed: expect 'RNFR' first.");
								break;
							case F_ISROOT:
								strcpy(sentence, "450 File operation failed: already at root.");
								break;
							default:
								strcpy(sentence, "450 File operation failed.");
								break;
						}
					}
				}
				// ---[REST]---  350
				else if (strcmp(commandArgs[0], "REST") == 0) {
					if (checkArgsNum(sentence, commandArgs[0], commandArgsNumber, 1, 1) == 0) {
						REST(commandArgs[1], sentence, &fileShift);
					}
				}
				// ---[???]---
				else {
					strcpy(sentence, "500 Invalid command: invalid command.");
				}
			}
		} else {
			strcpy(sentence, "500 Invalid command: invalid command.");
		}
		// ** 写入套接字 **
		switch (writeSocket(connfd, sentence)) {
			case S_SUCCESS:
				break;
			case S_ERROR_WRITE:
				printf("Error write(): %s(%d)\nConnection closed.\n", strerror(errno), errno);
				if (connfd >= 0) close(connfd);
				if (listenfd >= 0) close(listenfd);
				return NULL;
		}
	}
	// *** 关闭套接字 ***
	printf("Connection closed.\n");
	if (connfd >= 0) close(connfd);
	if (listenfd >= 0) close(listenfd);
	return NULL;
}

/******************
函数：命令行参数解析
******************/
void handleCommandLineArgs(int argc,char *argv[]) {
	int i, isFound;
	// -port
	isFound = 0;
	for (i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], "-port") == 0) {
			isFound = 1;
			break;
		}
	if (isFound)
		serverPort = atoi(argv[i + 1]);
	else
		serverPort = DEFAULT_PORT;
	// -root
	isFound = 0;
	for (i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], "-root") == 0) {
			isFound = 1;
			break;
		}
	if (isFound)
		strcpy(serverRootPath, argv[i + 1]);
	else
		strcpy(serverRootPath, DEFAULT_ROOT);
	return;
}

/******************
函数：主函数
******************/
int main(int argc, char **argv) {
	int listenfd, connfd;
	// 创建监听套接字
	char strIP[32];
	if (getServerIP(strIP) != 0) {
		printf("Error: failed to get local IP.\n");
		return 1;
	}
	printf("Local IP: %s\n", strIP);
	serverIP = inet_addr(strIP);
	// serverIP = inet_addr("127.0.0.1");
	// 处理命令行参数
	handleCommandLineArgs(argc, argv);
	if ((listenfd = createListenSocket(INADDR_ANY, serverPort)) < 0) {
		printf("Error createListenSocket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}
	// 持续创建连接 多线程
	while (1) {
		// 进行连接
		if ((connfd = acceptConnection(listenfd)) < 0)
			continue;
		// 进行处理 派生线程
		pthread_t id;
		pthread_create(&id, NULL, handleConnection, (void*)&connfd);
		pthread_detach(id);
	}
	return 0;
}

