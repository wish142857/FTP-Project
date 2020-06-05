#ifndef CLIENT_H
#define CLIENT_H

#include "socket.h"

// 宏定义声明
#define MAX_FILE_NUMBER 100
#define DIR_BUFFER_PATH "dirbuffer"

// 变量声明
extern char information[MAXLEN];
extern char sentence[MAXLEN];

// 函数声明
int initConnection();
int startConnection(unsigned int IP, unsigned int port);
int endConnection();
int fetchConnection();
int USER(const char *argument);
int PASS(const char *argument);
int PORT(char *argument);
int PASV();
int RETR(char *argument, char *savePath);
int STOR(char *argument);
int SYST();
int TYPE(char *argument);
int QUIT();
int MKD(char *argument);
int CWD(char *argument);
int PWD();
int RMD(char *argument);
int LIST(char *argument);
int RNFR(char *argument);
int RNTO(char *argument);
int analyseFileList(char *sentence, char *buffer, char **list, int *number);
int formatIPandPort(char *str, unsigned int IP, unsigned int port);



#endif // CLIENT_H
