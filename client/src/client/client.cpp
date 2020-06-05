#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


#include <QDebug>
#include <QProgressBar>

#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include "client.h"


extern QProgressBar *progressBar;           // 进度条控件
// char argument[MAXLEN];					// 参数
char information[MAXLEN];					// 信息
char sentence[MAXLEN];						// 字段
char command[MAXLEN];						// 命令行
char *commandArgs[MAXNUM];					// 命令行参数
int commandArgsNumber;					// 命令行参数数量

int sockfd;    						// 连接套接字（指令流）
int listenfd, datafd;				// 监听套接字，数据套接字（数据流）
unsigned int connIP, connPort;		// 连接 IP/port
int isPort, isPasv;					// 主动模式/被动模式


/******************
函数：客户端进行初始化
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int initConnection() {
    memset(information, 0, sizeof(information));
    memset(sentence, 0, sizeof(sentence));
    memset(command, 0, sizeof(command));
    commandArgsNumber = 0;
    sockfd = listenfd = datafd = -1;
    connIP = connPort = 0;
    isPort = isPasv = 0;
    strcpy(information, "【成功】客户端初始化完毕");
    return 0;
}

/******************
函数：客户端连接开始
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int startConnection(unsigned int IP, unsigned int port) {
    // *** 创建套接字/初始化/连接目标主机 ***
    sockfd = createConnectionSocket(IP, port);
    switch (sockfd) {
        case S_ERROR_SOCKET:
            sprintf(information, "【错误】套接字创建失败: %s(%d)", strerror(errno), errno);
            return -1;
        case S_ERROR_CONNECt:
            sprintf(information, "【错误】进行连接失败: %s(%d)", strerror(errno), errno);
            return -1;
        default:
            break;
    }
    // 读取初始消息
    if(readSocket(sockfd, sentence) < 0) {
        sprintf(information, "【错误】报文读取错误: %s(%d)", strerror(errno), errno);
        return -1;
    }
    strcpy(information, "【成功】已连接到服务端");
    return 0;
}


/******************
函数：客户端连接结束
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int endConnection() {
    // *** 关闭套接字/初始化 ***
    if (sockfd >= 0)
        close(sockfd);
    if (listenfd >= 0)
        close(listenfd);
    if (datafd >= 0)
        close(datafd);
    initConnection();
    return 0;
}


/******************
函数：客户端信息交互
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int fetchConnection() {
    if(writeSocket(sockfd, sentence) < 0) {
        sprintf(information, "【错误】报文写入错误: %s(%d)", strerror(errno), errno);
        return -1;
    }
    if(readSocket(sockfd, sentence) < 0) {
        sprintf(information, "【错误】报文读取错误: %s(%d)", strerror(errno), errno);
        return -1;
    }
    return 0;
}


/******************
函数：USER 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int USER(const char *argument) {
    // 命令发送 响应获取
    sprintf(sentence, "USER %s", argument);
    if (fetchConnection() == -1)
        return -1;
    // 响应解析
    int responseCode = getResponseCode(sentence);
    if (responseCode <= 0) {
        sprintf(information, "【错误】服务端响应格式错误: %s", sentence);
        return -1;
    }
    // 响应处理
    if (responseCode != 331) {
        sprintf(information, "【错误】USER 命令执行失败: \"%s\"", sentence);
        return -1;
    }
    strcpy(information, "【成功】USER 执行成功");
    return 0;
}


/******************
函数：PASS 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int PASS(const char *argument) {
    // 命令发送 响应获取
    sprintf(sentence, "PASS %s", argument);
    if (fetchConnection() == -1)
        return -1;
    // 响应解析
    int responseCode = getResponseCode(sentence);
    if (responseCode <= 0) {
        sprintf(information, "【错误】服务端响应格式错误: %s", sentence);
        return -1;
    }
    // 响应处理
    if (responseCode != 230) {
        sprintf(information, "【错误】PASS 命令执行失败: \"%s\"", sentence);
        return -1;
    }
    strcpy(information, "【成功】PASS 命令执行成功");
    return 0;
}

/******************
函数：PORT 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int PORT(char *argument) {
    // 参数格式检测
    if (analyseIPandPort(argument, &connIP, &connPort) == -1) {
        strcpy(information, "【错误】参数格式错误");
        return -1;
    }
    // 创建监听套接字
    if (listenfd >= 0) close(listenfd);
    listenfd = createListenSocket(INADDR_ANY, connPort);
    switch (listenfd) {
        case S_ERROR_SOCKET:
            sprintf(information, "【错误】套接字创建失败: %s(%d)", strerror(errno), errno);
            return -1;
        case S_ERROR_BIND:
            sprintf(information, "【错误】套接字绑定失败: %s(%d)", strerror(errno), errno);
            return -1;
        case S_ERROR_LISTEN:
            sprintf(information, "【错误】套接字监听失败: %s(%d)", strerror(errno), errno);
            return -1;
        default:
            break;
    }
    // 命令发送 响应获取
    sprintf(sentence, "PORT %s", argument);
    if (fetchConnection() == -1)
        return -1;
    // 响应解析
    int responseCode = getResponseCode(sentence);
    if (responseCode <= 0) {
        sprintf(information, "【错误】服务端响应格式错误: %s", sentence);
        return -1;
    }
    // 响应处理
    if (responseCode != 200) {
        sprintf(information, "【错误】PORT 命令执行失败: \"%s\"", sentence);
        return -1;
    }
    strcpy(information, "【成功】PORT 命令执行成功");
    isPort = 1;
    isPasv = 0;
    return 0;
}

/******************
函数：PASV 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int PASV() {
    // 命令发送 响应获取
    sprintf(sentence, "PASV");
    if (fetchConnection() == -1)
        return -1;
    // 响应解析
    int responseCode = getResponseCode(sentence);
    if (responseCode <= 0) {
        sprintf(information, "【错误】服务端响应格式错误: %s", sentence);
        return -1;
    }
    if (responseCode == 227) {
        if (analyseCommand(sentence, command, commandArgs, &commandArgsNumber) == -1) {
            strcpy(information, "【错误】服务端响应格式错误");
            return -1;
        }
        // % 格式：括号内为 (IP/Port)
        int l = 0, r = 0, t = 0;
        char buffer[32];
        while(sentence[l] != '(') {
            if (++l >= static_cast<int>(strlen(sentence))) {
                strcpy(information, "【错误】服务端响应格式错误");
                return -1;
            }
        }
        while(sentence[r] != ')') {
            if (++r >= static_cast<int>(strlen(sentence))) {
                strcpy(information, "【错误】服务端响应格式错误");
                return -1;
            }
        }
        if ((l - r) > 30) {
            strcpy(information, "【错误】服务端响应格式错误");
            return -1;
        }
        for (int i = l + 1; i < r; i++)
            buffer[t++] = sentence[i];
        buffer[t] = '\0';
        if (analyseIPandPort(buffer, &connIP, &connPort) == -1) {
            strcpy(information, "【错误】服务端响应格式错误");
            return -1;
        }
    } else {
        sprintf(information, "【错误】PASV 执行失败: \"%s\"", sentence);
        return -1;
    }
    strcpy(information, "【成功】PASV 执行成功");
    isPort = 0;
    isPasv = 1;
    return 0;
}

/******************
函数：RETR 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int RETR(char *argument, char *savePath) {
    // 命令发送
    sprintf(sentence, "RETR %s", argument);
    if(writeSocket(sockfd, sentence) < 0) {
        sprintf(information, "【错误】报文写入错误: %s(%d)", strerror(errno), errno);
        return -1;
    }
    // 尝试连接
    if (isPort) {
        // 主动模式
        if ((datafd = acceptConnection(listenfd)) < 0) {
            strcpy(information, "【错误】套接字创建错误");
            isPort = 0;
            isPasv = 0;
            return -1;
        }
    } else {
        // 被动模式
        if ((datafd = createConnectionSocket(connIP, connPort)) < 0) {
            strcpy(information, "【错误】套接字创建错误");
            isPort = 0;
            isPasv = 0;
            return -1;
        }
    }
    // 响应获取
    if(readSocket(sockfd, sentence) < 0) {
        sprintf(information, "【错误】报文读取错误: %s(%d)", strerror(errno), errno);
        close(datafd);
        datafd = -1;
        return -1;
    }
    // 响应解析
    int responseCode = getResponseCode(sentence);
    if (responseCode <= 0) {
        sprintf(information, "【错误】服务端响应格式错误: %s", sentence);
        close(datafd);
        datafd = -1;
        return -1;
    }
    // 成功连接 传输文件
    if (responseCode == 150) {
        // 获取文件大小
        long totalSize = 0, left = strlen(sentence) - 1, right = strlen(sentence) - 1;
        while (left >= 0) {
            if (sentence[left] == '(') break;
            left--;
        }
        while (right >= 0) {
            if (sentence[right] == ')') break;
            right--;
        }
        if (right > left) {
            char buffer[right - left];
            strncpy(buffer, &sentence[left + 1], right - left - 1);
            totalSize = atol(buffer);
        }
        // 打开文件
        int file = open(savePath, O_WRONLY | O_CREAT | O_TRUNC, 0664);
            if (file < 0) {
            close(datafd);
            datafd = -1;
            strcpy(information, "【错误】文件打开错误");
            return -1;
        }
        // 持续传输
        progressBar->reset();
        int n = 0;
        long m = 0;
        char buf[MAX_TRANSFER_BYTE + 10];
        while ((n = read(datafd, buf, MAX_TRANSFER_BYTE)) > 0) {
            if (n <= 0)
                break;
            if (write(file, buf, n) < 0) {
                close(file);
                close(datafd);
                datafd = -1;
                strcpy(information, "【错误】文件写入错误");
                return -1;
            }
            m += n;
            if (totalSize <= 0) {
                progressBar->setValue(100);
            } else {
                double x = static_cast<double>(m);
                double y = static_cast<double>(totalSize);
                int p = static_cast<int>(x * 100.0 / y);
                if ((p < 0) || (p > 100))
                    p = 100;
                progressBar->setValue(p);
            }
        }
        close(file);
        close(datafd);
        datafd = -1;
        // 响应获取
        if(readSocket(sockfd, sentence) < 0) {
            sprintf(information, "【错误】报文读取错误: %s(%d)", strerror(errno), errno);
            return -1;
        }
        // 响应解析
        responseCode = getResponseCode(sentence);
        if (responseCode <= 0) {
            strcpy(information, "【错误】服务端响应格式错误");
            return -1;
        }
        // 响应处理
        if (responseCode == 226) {
            strcpy(information, "【成功】RETR 命令执行成功");
            return 0;
        } else {
            sprintf(information, "【失败】RETR 命令执行失败: %s", sentence);
            return -1;
        }
    } else {
        sprintf(information, "【失败】RETR 命令执行失败: %s", sentence);
        return -1;
    }
}


/******************
函数：STOR 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int STOR(char *argument) {
    // 提取文件名
    char *filename;
    int i = strlen(argument) - 1;
    while (i >= 0) {
        if (argument[i] == '/')
            break;
        i--;
    }
    if (i >= 0)
        filename = &argument[i + 1];
    else
        filename = argument;
    // 命令发送
    sprintf(sentence, "STOR %s", filename);
    if(writeSocket(sockfd, sentence) < 0) {
        sprintf(information, "【错误】报文写入错误: %s(%d)", strerror(errno), errno);
        return -1;
    }
    // 尝试连接
    if (isPort) {
        // 主动模式
        if ((datafd = acceptConnection(listenfd)) < 0) {
            strcpy(information, "【错误】套接字创建错误");
            isPort = 0;
            isPasv = 0;
            return -1;
        }
    } else {
        // 被动模式
        if ((datafd = createConnectionSocket(connIP, connPort)) < 0) {
            strcpy(information, "【错误】套接字创建错误");
            isPort = 0;
            isPasv = 0;
            return -1;
        }
    }
    // 响应获取
    if(readSocket(sockfd, sentence) < 0) {
        sprintf(information, "【错误】报文读取错误: %s(%d)", strerror(errno), errno);
        close(datafd);
        datafd = -1;
        return -1;
    }
    // 响应解析
    int responseCode = getResponseCode(sentence);
    if (responseCode <= 0) {
        sprintf(information, "【错误】服务端响应格式错误: %s", sentence);
        close(datafd);
        datafd = -1;
        return -1;
    }
    // 成功连接 传输文件
    if (responseCode == 150) {
        // 打开文件
        int file = open(argument, O_RDONLY);
            if (file < 0) {
            close(datafd);
            datafd = -1;
            strcpy(information, "【错误】文件打开错误");
            return -1;
        }
        // 获取文件大小
        struct stat s_stat;
        stat(argument, &s_stat);
        long totalSize = s_stat.st_size;
        // 持续传输
        progressBar->reset();
        int n = 0, p = 0, l = 0;
        long m = 0;
        char buf[MAX_TRANSFER_BYTE + 10];
        while((n = read(file, buf, MAX_TRANSFER_BYTE)) > 0) {
            p = 0;
            while (p < n) {
                l = write(datafd, buf, n);
                if (l < 0) {
                    close(file);
                    close(datafd);
                    strcpy(information, "【错误】报文写入错误");
                    return -1;
                }
                p += l;
            }
            m += n;
            if (totalSize <= 0) {
                progressBar->setValue(100);
            } else {
                double x = static_cast<double>(m);
                double y = static_cast<double>(totalSize);
                int p = static_cast<int>(x * 100.0 / y);
                if ((p < 0) || (p > 100))
                    p = 100;
                progressBar->setValue(p);
            }
        }
        close(file);
        close(datafd);
        datafd = -1;
        // 响应获取
        if(readSocket(sockfd, sentence) < 0) {
            sprintf(information, "【错误】报文读取错误: %s(%d)", strerror(errno), errno);
            return -1;
        }
        // 响应解析
        responseCode = getResponseCode(sentence);
        if (responseCode <= 0) {
        sprintf(information, "【错误】服务端响应格式错误: %s", sentence);
            return -1;
        }
        // 响应处理
        if (responseCode == 226) {
            strcpy(information, "【成功】STOR 命令执行成功");
            return 0;
        } else {
            sprintf(information, "【失败】STOR 命令执行失败: %s", sentence);
            return -1;
        }
    } else {
        sprintf(information, "【失败】STOR 命令执行失败: %s", sentence);
        return -1;
    }
}

/******************
函数：SYST 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int SYST() {
    // 命令发送 响应获取
    sprintf(sentence, "SYST");
    if (fetchConnection() == -1)
        return -1;
    // 响应解析
    int responseCode = getResponseCode(sentence);
    if (responseCode <= 0) {
        sprintf(information, "【错误】服务端响应格式错误: %s", sentence);
        return -1;
    }
    // 响应处理
    if (responseCode != 215) {
        sprintf(information, "【错误】服务端响应格式错误: %s", sentence);
        return -1;
    }
    strcpy(information, "【成功】SYST 命令执行成功");
    return 0;
}


/******************
函数：TYPE 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int TYPE(char *argument) {
    // 命令发送 响应获取
    sprintf(sentence, "TYPE %s", argument);
    if (fetchConnection() == -1)
        return -1;
    // 响应解析
    int responseCode = getResponseCode(sentence);
    if (responseCode <= 0) {
        sprintf(information, "【错误】服务端响应格式错误: %s", sentence);
        return -1;
    }
    // 响应处理
    if (responseCode != 200) {
        sprintf(information, "【失败】TYPE 命令执行失败: %s", sentence);
        return -1;
    }
    strcpy(information, "【成功】TYPE 命令执行成功");
    return 0;
}


/******************
函数：QUIT 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int QUIT() {
    // 命令发送 响应获取
    sprintf(sentence, "QUIT");
    if (fetchConnection() == -1)
        return -1;
    // 响应解析
    int responseCode = getResponseCode(sentence);
    if (responseCode <= 0) {
        sprintf(information, "【错误】服务端响应格式错误: %s", sentence);
        return -1;
    }
    // 响应处理
    // TODO
    strcpy(information, "【成功】QUIT 命令执行成功");
    return 0;
}

/******************
函数：MKD 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int MKD(char *argument) {
    // 命令发送 响应获取
    sprintf(sentence, "MKD %s", argument);
    if (fetchConnection() == -1)
        return -1;
    // 响应解析
    int responseCode = getResponseCode(sentence);
    if (responseCode <= 0) {
        sprintf(information, "【错误】服务端响应格式错误: %s", sentence);
        return -1;
    }
    // 响应处理
    if (responseCode != 257) {
        sprintf(information, "【失败】MKD 命令执行失败: %s", sentence);
        return -1;
    }
    strcpy(information, "【成功】MKD 命令执行成功");
    return 0;
}

/******************
函数：CWD 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int CWD(char *argument) {
    // 命令发送 响应获取
    sprintf(sentence, "CWD %s", argument);
    if (fetchConnection() == -1)
        return -1;
    // 响应解析
    int responseCode = getResponseCode(sentence);
    if (responseCode <= 0) {
        sprintf(information, "【错误】服务端响应格式错误: %s", sentence);
        return -1;
    }
    // 响应处理
    if (responseCode != 250) {
        sprintf(information, "【失败】CWD 命令执行失败: %s", sentence);
        return -1;
    }
    strcpy(information, "【成功】CWD 命令执行成功");
    return 0;
}

/******************
函数：PWD 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int PWD() {
    // 命令发送 响应获取
    sprintf(sentence, "PWD");
    if (fetchConnection() == -1)
        return -1;
    // 响应解析
    int responseCode = getResponseCode(sentence);
    if (responseCode <= 0) {
        sprintf(information, "【错误】服务端响应格式错误: %s", sentence);
        return -1;
    }
    // 响应处理
    if (responseCode != 257) {
        sprintf(information, "【失败】PWD 命令执行失败: %s", sentence);
        return -1;
    }
    strcpy(information, "【成功】PWD 命令执行成功");
    return 0;
}

/******************
函数：RMD 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int RMD(char *argument) {
    // 命令发送 响应获取
    sprintf(sentence, "RMD %s", argument);
    if (fetchConnection() == -1)
        return -1;
    // 响应解析
    int responseCode = getResponseCode(sentence);
    if (responseCode <= 0) {
        sprintf(information, "【错误】服务端响应格式错误: %s", sentence);
        return -1;
    }
    // 响应处理
    if (responseCode != 250) {
        sprintf(information, "【失败】RMD 命令执行失败: %s", sentence);
        return -1;
    }
    strcpy(information, "【成功】RMD 命令执行成功");
    return 0;
}

/******************
函数：LIST 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int LIST(char *argument) {
    // 调用 PASV 命令
    if (PASV() != 0)
        return -1;
    // 命令发送
    if (!argument)
        sprintf(sentence, "LIST");
    else
        sprintf(sentence, "LIST %s", argument);
    if(writeSocket(sockfd, sentence) < 0) {
        sprintf(information, "【错误】报文写入错误: %s(%d)", strerror(errno), errno);
        return -1;
    }
    // 被动模式 尝试连接
    if ((datafd = createConnectionSocket(connIP, connPort)) < 0) {
        strcpy(information, "【错误】套接字创建错误");
        isPort = 0;
        isPasv = 0;
        return -1;
    }
    // 响应获取
    if(readSocket(sockfd, sentence) < 0) {
        sprintf(information, "【错误】报文读取错误: %s(%d)", strerror(errno), errno);
        close(datafd);
        datafd = -1;
        return -1;
    }
    // 响应解析
    int responseCode = getResponseCode(sentence);
    if (responseCode <= 0) {
        sprintf(information, "【错误】服务端响应格式错误: %s", sentence);
        close(datafd);
        datafd = -1;
        return -1;
    }
    // 成功连接 传输文件
    if (responseCode == 150) {
        // 打开文件
        int file = open(DIR_BUFFER_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0664);
            if (file < 0) {
            close(datafd);
            datafd = -1;
            strcpy(information, "【错误】文件打开错误");
            return -1;
        }
        // 持续传输
        int n = 0;
        char buf[MAX_TRANSFER_BYTE + 10];
        while ((n = read(datafd, buf, MAX_TRANSFER_BYTE)) > 0) {
            if (n <= 0)
                break;
            if (write(file, buf, n) < 0) {
                close(file);
                close(datafd);
                datafd = -1;
                strcpy(information, "【错误】文件写入错误");
                return -1;
            }
        }
        close(file);
        close(datafd);
        datafd = -1;
        // 响应获取
        if(readSocket(sockfd, sentence) < 0) {
            sprintf(information, "【错误】报文读取错误: %s(%d)", strerror(errno), errno);
            return -1;
        }
        // 响应解析
        responseCode = getResponseCode(sentence);
        if (responseCode <= 0) {
            strcpy(information, "【错误】服务端响应格式错误");
            return -1;
        }
        // 响应处理
        if (responseCode == 226) {
            strcpy(information, "【成功】LIST 命令执行成功");
            return 0;
        } else {
            sprintf(information, "【失败】LIST 命令执行失败: %s", sentence);
            return -1;
        }
    } else {
        sprintf(information, "【失败】LIST 命令执行失败: %s", sentence);
        return -1;
    }
}


/******************
函数：RNFR 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int RNFR(char *argument) {
    // 命令发送 响应获取
    sprintf(sentence, "RNFR %s", argument);
    if (fetchConnection() == -1)
        return -1;
    // 响应解析
    int responseCode = getResponseCode(sentence);
    if (responseCode <= 0) {
        sprintf(information, "【错误】服务端响应格式错误: %s", sentence);
        return -1;
    }
    // 响应处理
    if (responseCode != 350) {
        sprintf(information, "【失败】RNFR 命令执行失败: %s", sentence);
        return -1;
    }
    strcpy(information, "【成功】RNFR 命令执行成功");
    return 0;
}

/******************
函数：RNTO 命令
返回：0: 成功  -1: 失败
成功信息/失败信息储存于 information
******************/
int RNTO(char *argument) {
    // 命令发送 响应获取
    sprintf(sentence, "RNTO %s", argument);
    if (fetchConnection() == -1)
        return -1;
    // 响应解析
    int responseCode = getResponseCode(sentence);
    if (responseCode <= 0) {
        sprintf(information, "【错误】服务端响应格式错误: %s", sentence);
        return -1;
    }
    // 响应处理
    if (responseCode != 250) {
        sprintf(information, "【失败】RNTO 命令执行失败: %s", sentence);
        return -1;
    }
    strcpy(information, "【成功】RNTO 命令执行成功");
    return 0;
}


/******************
函数：解析文件列表
******************/
int analyseFileList(char *sentence, char *buffer, char **list, int *number) {
    // 命令行参数获取
    strcpy(buffer, sentence);
    *number = 0;
    int i = 0, j = 0, length = static_cast<int>(strlen(buffer));
    char delimiter = ' ';
    while (i < length) {
        while (buffer[i] == delimiter)
            if(++i >= length) break;
        if (i >= length) break;
        j = i + 1;
        while (buffer[j] != delimiter)
            if(++j >= length) break;
        if (*number >= MAX_FILE_NUMBER)
            return -1;
        list[*number] = &buffer[i];
        *number += 1;
        buffer[j] = '\0';
        i = j + 1;
    }
    if (*number <= 0)
        return -1;
    return 0;
}



/******************
函数：客户端连接交互（键盘输入）
******************/
int handleConnection(int connfd) {
    // *** 持续连接交互 ***
    while(1) {
        // ** 获取键盘输入 **
        printf("TCP> ");
        fgets(sentence, 4096, stdin);
        int sentenceLength = strlen(sentence);
        while (sentence[sentenceLength - 1] == '\n') {
            sentence[--sentenceLength] = '\0';
        }
        // ** 命令筛选&处理 **
        if (strcmp(sentence, "EXIT") == 0)
            break;
        if (fetchConnection() < 0) {
                printf("Connection closed.\n");
                close(listenfd);
                return -1;
        }
        printf("FROM SERVER: %s\n", sentence);
    }
    // *** 关闭连接 ***
    if (listenfd >= 0)
        close(listenfd);
    if (datafd >= 0)
        close(datafd);
    close(connfd);
    return 0;
}


/******************
函数：序列测试
******************/
/*
int test() {
    // 开始连接
    if (startConnection(inet_addr("127.0.0.1"), DEFAULT_PORT) < 0) {
        printf("%s\n", information);
        return 1;
    }
    // 接受初始消息  打印初始消息
    if(readSocket(sockfd, sentence) < 0) {
        sprintf(information, "【错误】报文读取错误: %s(%d)", strerror(errno), errno);
        return 1;
    }
    printf("%s\n", sentence);
    // 进行序列测试
    USER("anonymous");
    printf("FROM SERVER: %s    INFORMATION: %s\n", sentence, information);
    PASS("AAAA");
    printf("FROM SERVER: %s    INFORMATION: %s\n", sentence, information);

    PORT("127,0,0,1,30,0");
    printf("FROM SERVER: %s    INFORMATION: %s\n", sentence, information);

    RETR("pic");
    printf("FROM SERVER: %s    INFORMATION: %s\n", sentence, information);

    PASV();
    printf("FROM SERVER: %s    INFORMATION: %s\n", sentence, information);
    RETR("pic");
    printf("FROM SERVER: %s    INFORMATION: %s\n", sentence, information);

    return 0;
}
*/
