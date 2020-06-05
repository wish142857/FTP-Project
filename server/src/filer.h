#ifndef FILER_H
#define FILER_H

// 宏定义
#define MAX_ROOT_PATH_LENGTH 128
#define MAX_WORK_PATH_LENGTH 8192
#define DEFAULT_ROOT "/tmp"		// 默认文件路径

#define F_SUCCESS   0	// 执行成功
#define F_ERROR    -1 	// 执行失败  错误信息存于errno
#define F_OVERFLOW -2	// 路径过长
#define F_NOTFOUND -3	// 无效路径
#define F_NORENAME -4	// 未处于重命名状态
#define F_ISROOT   -5   // 已处于根目录

// 函数声明
int getCompletePath(char *rootPath, char *workPath, char *targetPath, char *completePath);			// 获取完整目录路径
int makeDirectory(char *rootPath, char *workPath, char *targetPath);								// 创建目录（MKD）
int removeDirectory(char *rootPath, char *workPath, char *targetPath);								// 删除目录（RMD）
int changeWorkDirectory(char *rootPath, char *workPath, char *targetPath);							// 改变工作目录（CWD）
int printWorkDirectory(char *rootPath, char *workPath, char *sentence);								// 打印工作目录（PWD）
int listDirectory(char *rootPath, char *workPath, char *targetPath, char *sentence);				// 列出文件信息或文件列表（LIST）
int renameFR(char *rootPath, char *workPath, char *targetPath, char *renamePath, int *isRenaming);	// 对旧路径重命名（RNFR）
int renameTO(char *rootPath, char *workPath, char *targetPath, char *renamePath, int *isRenaming);	// 对新路径重命名（RNTO）

#endif
