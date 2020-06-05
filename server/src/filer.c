#include <sys/stat.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <time.h>


#include "filer.h"

/******************
注意：以下函数需要如下参数
    char rootPath[MAX_ROOT_PATH_LENGTH];    // 根路径
    char workPath[MAX_WORK_PATH_LENGTH];    // 工作路径
    char targetPath[MAX_WORK_PATH_LENGTH];  // 目标路径
    char renamePath[MAX_WORK_PATH_LENGTH];  // 重命名路径
    int isRenaming = 0;                     // 是否处于重命名状态
    char sentence[MAXLEN];                  // 字段
******************/



/******************
函数：获取上级目录路径
******************/
int getUpPath(char *workPath) {
	int i = strlen(workPath);
	if (i <= 0)
		return F_ISROOT;
	while (i > 0)
		if (workPath[--i] == '/')
			break;
	workPath[i] = '\0';
	return F_SUCCESS;
}


/******************
函数：获取完整目录路径
******************/
int getCompletePath(char *rootPath, char *workPath, char *targetPath, char *completePath) {
    if (!rootPath || !workPath)
        return F_NOTFOUND;
    if (!targetPath || (strlen(targetPath) <= 0)) {
        // 空参数
        sprintf(completePath, "%s%s", rootPath, workPath);
        return F_SUCCESS;
    }
    if (targetPath[0] == '/') {
        // 绝对路径
        sprintf(completePath, "%s%s", rootPath, targetPath);
        return F_SUCCESS;
    } else {
        // 相对路径
        if (strlen(workPath) + strlen(targetPath) + 1 >= MAX_WORK_PATH_LENGTH)
            return F_OVERFLOW;
        sprintf(completePath, "%s%s/%s", rootPath, workPath, targetPath);
        return F_SUCCESS;
    }
}


/******************
函数：创建目录（MKD）
// TODO 循环创建目录
******************/
int makeDirectory(char *rootPath, char *workPath, char *targetPath) {
    // 获取完整路径
    char completePath[MAX_ROOT_PATH_LENGTH + MAX_WORK_PATH_LENGTH];
    int ret = getCompletePath(rootPath, workPath, targetPath, completePath);
    if (ret < 0)
        return ret;
    // 创建目录
    return mkdir(completePath, 0777);
}


/******************
函数：删除目录（RMD）
******************/
int removeDirectory(char *rootPath, char *workPath, char *targetPath) {
    // 获取完整路径
    char completePath[MAX_ROOT_PATH_LENGTH + MAX_WORK_PATH_LENGTH];
    int ret = getCompletePath(rootPath, workPath, targetPath, completePath);
    if (ret < 0)
        return ret;
    // 删除目录
    return rmdir(completePath);
}


/******************
函数：改变工作目录（CWD）
******************/
int changeWorkDirectory(char *rootPath, char *workPath, char *targetPath) {
    if (!targetPath || (strlen(targetPath) < 0))
        return F_SUCCESS;
    // 特殊路径处理
    if (strcmp(targetPath, ".") == 0)
		return F_SUCCESS;
    if (strcmp(targetPath, "..") == 0)
		return getUpPath(workPath);
    // 获取完整路径
    char completePath[MAX_ROOT_PATH_LENGTH + MAX_WORK_PATH_LENGTH];
    int ret = getCompletePath(rootPath, workPath, targetPath, completePath);
    if (ret < 0)
        return ret;
    // 判断路径存在性
    if (access(completePath, F_OK) == 0) {
        if (targetPath[0] == '/')
            sprintf(workPath, "%s", targetPath);
        else
            sprintf(workPath, "%s/%s", workPath, targetPath);
        return F_SUCCESS;
    } else {
        return F_NOTFOUND;
    }
}


/******************
函数：打印工作目录（PWD）
******************/
int printWorkDirectory(char *rootPath, char *workPath, char *sentence) {
    // 生成完整路径
    sprintf(sentence, "257 \"%s%s\"", rootPath, workPath);
    return 0;
}


/******************
函数：列出文件信息或文件列表（LIST）
返回：F_SUCCESS|F_ERROR|F_NOTFOUND
******************/
int listDirectory(char *rootPath, char *workPath, char *targetPath, char *sentence) {
    // 获取完整路径
    char completePath[MAX_ROOT_PATH_LENGTH + MAX_WORK_PATH_LENGTH];
    int ret = getCompletePath(rootPath, workPath, targetPath, completePath);
    if (ret < 0)
        return ret;
    // 判断路径存在性
    if (access(completePath, F_OK) != 0)
        return F_NOTFOUND;
    // 获取路径信息
    struct stat s_stat;                     // 文件路径信息
    stat(completePath, &s_stat);            // 获取路径信息
    // 判断路径类型
    memset(sentence, 0, sizeof(sentence));
    if (S_ISDIR(s_stat.st_mode)) {                          // 目录文件
        // 遍历目录，写入文件列表
        DIR *pDir = NULL;
        struct dirent *pEnt = NULL;
        if ((pDir = opendir(completePath)) == NULL)         // 打开目录指针
            return F_ERROR;
        while ((pEnt = readdir(pDir)) != NULL) {            // 通过目录指针读目录
            if ((strcmp(pEnt->d_name, ".") == 0) || (strcmp(pEnt->d_name, "..") == 0))
                continue;
            char path[sizeof(completePath) + sizeof(pEnt->d_name)];
            sprintf(path, "%s/%s", completePath, pEnt->d_name);
            stat(path, &s_stat);                            // 获取路径信息
            char type[16];
            if (S_ISDIR(s_stat.st_mode))
                strcpy(type, "d---------");
            else if (S_ISREG(s_stat.st_mode))
                strcpy(type, "----------");                
            else continue;
            time_t t = s_stat.st_mtime;
            struct tm *p = gmtime(&t);
            char s[64];
            strftime(s, sizeof(s), "%m %d %H:%M:%S", p);
            sprintf(sentence, "%s%s %d %d %d %ld %s %s\n", sentence, type, 0, 0, 0, s_stat.st_size, s, pEnt->d_name);
        }
        return closedir(pDir);                              // 关闭目录
    }
    else if (S_ISREG(s_stat.st_mode)) {                     // 普通文件
        time_t t = s_stat.st_mtime;
        struct tm *p = gmtime(&t);
        char s[64];
        strftime(s, sizeof(s), "%m %d %H:%M:%S", p);
        sprintf(sentence, "---------- %d %d %d %ld %s %s\n", 0, 0, 0, s_stat.st_size, s, targetPath);
        return F_SUCCESS;
    }
    return F_SUCCESS;
}


/******************
函数：对旧路径重命名（RNFR）
返回：0: 成功  -1: 失败  -2: 溢出
******************/
int renameFR(char *rootPath, char *workPath, char *targetPath, char *renamePath, int *isRenaming) {
    // 获取完整路径
    char completePath[MAX_ROOT_PATH_LENGTH + MAX_WORK_PATH_LENGTH];
    int ret = getCompletePath(rootPath, workPath, targetPath, completePath);
    if (ret < 0)
        return ret;
    // 判断路径存在性
    if (access(completePath, F_OK) != 0)
        return F_NOTFOUND;
    // 记录重命名路径
    sprintf(renamePath, "%s", completePath);
    *isRenaming = 1;
    return F_SUCCESS;
}


/******************
函数：对新路径重命名（RNTO）
返回：0: 成功  -1: 失败  -2: 溢出
******************/
int renameTO(char *rootPath, char *workPath, char *targetPath, char *renamePath, int *isRenaming) {
    // 获取完整路径
    char completePath[MAX_ROOT_PATH_LENGTH + MAX_WORK_PATH_LENGTH];
    int ret = getCompletePath(rootPath, workPath, targetPath, completePath);
    if (ret < 0)
        return ret;
    // 进行重命名
    if (*isRenaming) {
        *isRenaming = 0;
        return rename(renamePath, completePath);
    }
    return F_NORENAME;
}

