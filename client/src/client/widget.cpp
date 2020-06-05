#include "widget.h"
#include "ui_widget.h"
#include <QDebug>
#include <QString>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>

#include "client.h"
using namespace std;


QProgressBar *progressBar;


Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    // 加载 UI
    ui->setupUi(this);
    strcpy(information, "【提示】正在进行客户端初始化...");
    updateTextBrowser(information, 1);
    // 控件属性设定
    this->setWindowTitle(("iFile - Client"));   // 设置窗口标题
    this->setFixedSize(600, 900);                // 固定窗口大小
    ui->tableWidget->setRowCount(0);             // 设置行数
    ui->tableWidget->setColumnCount(4);          // 设置列数
    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);                // 设置表格只读
    ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);              // 设置限定选择单行
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);               // 设置选择模式为单行
    QStringList headers;
    headers << QStringLiteral("名称") << QStringLiteral("类型") << QStringLiteral("大小") << QStringLiteral("时间");
    ui->tableWidget->setHorizontalHeaderLabels(headers);                                // 设置表头内容
    ui->tableWidget->setColumnWidth(0, 150);
    ui->tableWidget->setColumnWidth(1, 100);
    ui->tableWidget->setColumnWidth(2, 100);
    ui->tableWidget->setColumnWidth(3, 150);
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);                   // 设置表头自适应宽度
    ui->progressBar->setRange(0, 100);
    progressBar = ui->progressBar;
    // 信号-槽关联
    connect(ui->buttonConnect, SIGNAL(clicked()), this, SLOT(connecting()));
    connect(ui->buttonLogin, SIGNAL(clicked()), this, SLOT(logining()));
    connect(ui->buttonMake, SIGNAL(clicked()), this, SLOT(making()));
    connect(ui->buttonRemove, SIGNAL(clicked()), this, SLOT(removing()));
    connect(ui->buttonRename, SIGNAL(clicked()), this, SLOT(renaming()));
    connect(ui->buttonUpload, SIGNAL(clicked()),this, SLOT(uploading()));
    connect(ui->buttonDownload, SIGNAL(clicked()),this, SLOT(downloading()));
    connect(ui->tableWidget, SIGNAL(itemClicked(QTableWidgetItem*)), this, SLOT(clickTableItem(QTableWidgetItem*)));
    connect(ui->tableWidget, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this, SLOT(doubleclickTableItem(QTableWidgetItem*)));
    // 客户端初始化
    isConnect = isLogin = 0;
    initConnection();
    refresh();
    strcpy(information, "【成功】客户端初始化完毕");
    updateTextBrowser(information, 0);
}

Widget::~Widget()
{
    // 断开连接
    endConnection();
    delete ui;
}

/*********************
函数: 刷新函数
********************/
void Widget::refresh() {
    // 进度条刷新
    ui->progressBar->setValue(100);
    // 图标刷新
    if (isConnect)
        ui->picConnect->setStyleSheet("image: url(:/pic/connection-on.png)");
    else
        ui->picConnect->setStyleSheet("image: url(:/pic/connection-off.png)");
    if (isLogin)
        ui->picClient->setStyleSheet("image: url(:/pic/client-on.png)");
    else
        ui->picClient->setStyleSheet("image: url(:/pic/client-off.png)");
    // 文件列表刷新
    updateTable();
    return;
}

/*********************
函数: 更新文本框函数
********************/
void Widget::updateTextBrowser(char *text, int type) {
    switch (type) {
        case 0:
            ui->textBrowser->append("<font color=\"#00EE00\">" + QString(text) + "</font> ");
            break;
        case -1:
            ui->textBrowser->append("<font color=\"#FF0000\">" + QString(text) + "</font> ");
            break;
        case 1:
            ui->textBrowser->append("<font color=\"#FFD700\">" + QString(text) + "</font> ");
            break;
        default:
            ui->textBrowser->append("<font color=\"#00BFFF\">" + QString(text) + "</font> ");
            break;
    }
    return;
}

/*********************
函数: 更新表格函数
********************/
void Widget::updateTable() {
    // 清空表格
    clearTable();
    if ((!isConnect) || (!isLogin))
        return;
    // 调用 PWD/LIST 命令
    strcpy(information, "【提示】正在更新文件列表...");
    ui->progressBar->reset();
    updateTextBrowser(information, 1);
    if (PWD() != 0) {
        strcpy(information, "【失败】更新文件列表失败:");
        ui->progressBar->setValue(100);
        updateTextBrowser(information, -1);
        updateTextBrowser(sentence, 2);
        return;
    }
    int left = 0, right = static_cast<int>(strlen(sentence)) - 1;
    while (left <= right) {
        if(sentence[left] == '"') break;
        left++;
    }
    while (right >= 0) {
        if(sentence[right] == '"') break;
        right--;
    }
    if (right > left) {
        char buffer[right - left];
        for (int i = 1; i <= right - left - 1; i++)
            buffer[i - 1] = sentence[left + i];
        buffer[right - left - 1] = '\0';
        ui->labelPath->setText(QString(buffer));
    } else {
        ui->labelPath->setText("?");
    }
    if (LIST(nullptr) != 0) {
        strcpy(information, "【失败】更新文件列表失败:");
        ui->progressBar->setValue(100);
        updateTextBrowser(information, -1);
        updateTextBrowser(sentence, 2);
        return;
    }
    // 解析文件列表
    ifstream ifs(DIR_BUFFER_PATH);
    string s, delim(" "), name, type, size, time;
    vector<string>ans;
    addTableItem("..", "", "", "");
    while(getline(ifs, s))
    {
        // 脱去 \r \n
        if (s.length() <= 0) continue;
        if (s[s.length() - 1] == '\r') s.pop_back();
        if (s.length() <= 0) continue;
        if (s[s.length() - 1] == '\n') s.pop_back();
        // 字符串分割
        ans.clear();
        string::size_type pos_1, pos_2 = 0;
        while(pos_2 != s.npos){
            pos_1 = s.find_first_not_of(delim, pos_2);
            if(pos_1 == s.npos) break;
            pos_2 = s.find_first_of(delim, pos_1);
            ans.push_back(s.substr(pos_1, pos_2 - pos_1));
        }
        // 信息提取
        name = ans[ans.size() - 1];
        if (ans.size() >= 8) {
            size = ans[4] + " 字节";
            time = ans[5] + " " + ans[6] + " " + ans[7];
        } else {
            size = "? 字节";
            time = "? ? ?";
        }
        switch (ans[0][0]) {
            case '-':
                type = "文件";
                break;
            case 'd':
                type = "文件夹";
                size = "";
                break;
            default:
                continue;
        }
        addTableItem(name, type, size, time);
    }
    strcpy(information, "【成功】成功更新文件列表");
    ui->progressBar->setValue(100);
    updateTextBrowser(information, 0);
    return;
}

/*********************
函数: 清除表格函数
********************/
void Widget::clearTable() {
    ui->tableWidget->setRowCount(0);
    return;
}

/*********************
函数: 添加表格元素函数
********************/
void Widget::addTableItem(string filename, string filetype, string filesize, string filetime) {
    int rowIndex = ui->tableWidget->rowCount();
    ui->tableWidget->setRowCount(rowIndex + 1);
    ui->tableWidget->setItem(rowIndex, 0, new QTableWidgetItem(QString::fromStdString(filename)));
    ui->tableWidget->setItem(rowIndex, 1, new QTableWidgetItem(QString::fromStdString(filetype)));
    ui->tableWidget->setItem(rowIndex, 2, new QTableWidgetItem(QString::fromStdString(filesize)));
    ui->tableWidget->setItem(rowIndex, 3, new QTableWidgetItem(QString::fromStdString(filetime)));
    ui->tableWidget->item(rowIndex, 0)->setTextAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
    ui->tableWidget->item(rowIndex, 1)->setTextAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
    ui->tableWidget->item(rowIndex, 2)->setTextAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
    ui->tableWidget->item(rowIndex, 3)->setTextAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
    return;
}

/*********************
函数: 连接函数
********************/
void Widget::connecting() {
    strcpy(information, "【提示】正在连接...");
    ui->progressBar->reset();
    updateTextBrowser(information, 1);
    // 断开先前连接
    endConnection();
    isConnect = isLogin = 0;
    // 获取 IP 与 Port
    string strIP = ui->lineEditIP->text().toStdString();
    string strPort = ui->lineEditPort->text().toStdString(); 
    if (inet_addr(strIP.c_str()) == INADDR_NONE) {
        // IP 格式错误
        strcpy(information, "【错误】IP 格式错误");
        updateTextBrowser(information, -1);
    } else if (atoi(strPort.c_str()) <= 0) {
        // port 格式错误
        strcpy(information, "【错误】Port 格式错误");
        updateTextBrowser(information, -1);
    } else {
        unsigned int IP = inet_addr(strIP.c_str());
        unsigned int port = static_cast<unsigned int>(atoi(strPort.c_str()));
        // 进行连接
        if (startConnection(IP, port) == 0) {
            updateTextBrowser(sentence, 2);
            updateTextBrowser(information, 0);
            isConnect = 1;
        } else {
            updateTextBrowser(information, -1);
        }
    }
    // 界面刷新
    refresh();
    return;
}

/*********************
函数: 登录函数
********************/
void Widget::logining() {
    strcpy(information, "【提示】正在登录...");
    ui->progressBar->reset();
    updateTextBrowser(information, 1);
    // 检查是否已连接
    if (isConnect == 1) {
        // 获取 User 与 Pass
        string strUser = ui->lineEditUser->text().toStdString();
        string strPass = ui->lineEditEmail->text().toStdString();
        // 调用 USER 与 PASS 命令
        if (USER(strUser.c_str()) != 0) {
            updateTextBrowser(information, -1);
            strcpy(information, "【失败】登录失败");
            updateTextBrowser(information, -1);
            ui->progressBar->setValue(100);
            return;
        }
        if (PASS(strPass.c_str()) != 0) {
            updateTextBrowser(information, -1);
            strcpy(information, "【失败】登录失败");
            updateTextBrowser(information, -1);
            ui->progressBar->setValue(100);
            return;
        }
        isLogin = 1;
        strcpy(information, "【成功】已成功登录");
        updateTextBrowser(information, 0);
        // 调用 SYST 命令
        strcpy(information, "【信息】自动调用 SYST 命令...");
        updateTextBrowser(information, 1);
        SYST();
        updateTextBrowser(sentence, 2);
        // 调用 TYPE 命令
        strcpy(information, "【信息】自动调用 TYPE I 命令...");
        updateTextBrowser(information, 1);
        char type[] = "I";
        TYPE(type);
        updateTextBrowser(sentence, 2);
    } else {
        strcpy(information, "【错误】未连接到服务端");
        updateTextBrowser(information, -1);
    }
    // 界面刷新
    refresh();
    return;
}


/*********************
函数: 创建目录函数
********************/
void Widget::making() {
    // 登录状态判断
    if (!isConnect || !isLogin) {
        strcpy(information, "【提示】请先连接到服务器并登录");
        updateTextBrowser(information, 1);
        return;
    }
    ui->progressBar->reset();
    QByteArray ba = ui->lineEdit_1->text().toLatin1();
    char *ch = ba.data();
    sprintf(information, "【提示】正在创建目录 [%s]...", ch);
    updateTextBrowser(information, 1);
    // 调用 MKD 命令
    if (MKD(ch) != 0) {
        strcpy(information, "【失败】目录创建失败:");
        updateTextBrowser(information, -1);
        updateTextBrowser(sentence, 2);
    } else {
        strcpy(information, "【成功】目录创建成功");
        updateTextBrowser(information, 0);
    }
    // 界面刷新
    refresh();
    return;
}


/*********************
函数: 删除目录函数
********************/
void Widget::removing() {
    // 登录状态判断
    if (!isConnect || !isLogin) {
        strcpy(information, "【提示】请先连接到服务器并登录");
        updateTextBrowser(information, 1);
        return;
    }
    ui->progressBar->reset();
    QByteArray ba = ui->lineEdit_2->text().toLatin1();
    char *ch = ba.data();
    sprintf(information, "【提示】正在删除目录 [%s]...", ch);
    updateTextBrowser(information, 1);
    // 调用 RMD 命令
    if (RMD(ch) != 0) {
        strcpy(information, "【失败】目录删除失败:");
        updateTextBrowser(information, -1);
        updateTextBrowser(sentence, 2);
    } else {
        strcpy(information, "【成功】目录删除成功");
        updateTextBrowser(information, 0);
    }
    // 界面刷新
    refresh();
    return;
}

/*********************
函数: 切换目录函数
********************/
void Widget::changing(char *filename) {
    // 登录状态判断
    if (!isConnect || !isLogin) {
        strcpy(information, "【提示】请先连接到服务器并登录");
        updateTextBrowser(information, 1);
        return;
    }
    ui->progressBar->reset();
    sprintf(information, "【提示】正在切换目录 [%s]...", filename);
    updateTextBrowser(information, 1);
    // 调用 CWD 命令
    if (CWD(filename) != 0) {
        strcpy(information, "【失败】目录切换失败:");
        updateTextBrowser(information, -1);
        updateTextBrowser(sentence, 2);
    } else {
        strcpy(information, "【成功】目录切换成功");
        updateTextBrowser(information, 0);
    }
    // 界面刷新
    refresh();
    return;
}

/*********************
函数: 重命名目录函数
********************/
void Widget::renaming() {
    // 登录状态判断
    if (!isConnect || !isLogin) {
        strcpy(information, "【提示】请先连接到服务器并登录");
        updateTextBrowser(information, 1);
        return;
    }
    ui->progressBar->reset();
    QByteArray ba1 = ui->lineEdit_5->text().toLatin1();
    char *ch1 = ba1.data();
    QByteArray ba2 = ui->lineEdit_6->text().toLatin1();
    char *ch2 = ba2.data();
    sprintf(information, "【提示】正在重命名目录 [%s] → [%s]...", ch1, ch2);
    updateTextBrowser(information, 1);
    // 调用 RNFR/RNTO 命令
    if (RNFR(ch1) != 0) {
        strcpy(information, "【失败】目录重命名失败:");
        updateTextBrowser(information, -1);
        updateTextBrowser(sentence, 2);
        ui->progressBar->setValue(100);
        return;
    }
    if (RNTO(ch2) != 0) {
        strcpy(information, "【失败】目录重命名失败:");
        updateTextBrowser(information, -1);
        updateTextBrowser(sentence, 2);
        ui->progressBar->setValue(100);
        return;
    }
    // 界面刷新
    refresh();
    return;
}


/*********************
函数: 上传函数
********************/
void Widget::uploading() {
    // 登录状态判断
    if (!isConnect || !isLogin) {
        strcpy(information, "【提示】请先连接到服务器并登录");
        updateTextBrowser(information, 1);
        return;
    }
    // 目标获取
    QString fileName = QFileDialog::getOpenFileName(this, tr("上传文件"), ".");
    if (fileName == "") {
        strcpy(information, "【提示】请选择有效文件");
        updateTextBrowser(information, 1);
        return;
    }
    ui->lineEdit_3->setText(fileName);
    QByteArray ba = fileName.toLatin1();
    char *ch = ba.data();
    // 判断模式
    if (ui->radioButtonPort->isChecked()) {
        // 主动模式
        sprintf(information, "【信息】以主动模式上传 [%s]...", ch);
        updateTextBrowser(information, 1);
        ui->progressBar->reset();
        // 获取 IP 与 Port
        string strIP = ui->lineEditIP_2->text().toStdString();
        string strPort = ui->lineEditPort_2->text().toStdString();
        if (inet_addr(strIP.c_str()) == INADDR_NONE) {
            // IP 格式错误
            strcpy(information, "【错误】IP 格式错误");
            updateTextBrowser(information, -1);
            ui->progressBar->setValue(100);
            return;
        } else if (atoi(strPort.c_str()) <= 0) {
            // port 格式错误
            strcpy(information, "【错误】Port 格式错误");
            updateTextBrowser(information, -1);
            ui->progressBar->setValue(100);
            return;
        }
        // 调用 PORT 命令
        unsigned int IP = inet_addr(strIP.c_str());
        unsigned int port = static_cast<unsigned int>(atoi(strPort.c_str()));
        char str[32];
        if (formatIPandPort(str, IP, port) == -1) {
            strcpy(information, "【错误】IP/Port 格式错误");
            updateTextBrowser(information, -1);
            ui->progressBar->setValue(100);
            return;
        }
        if (PORT(str) != 0) {
            updateTextBrowser(information, -1);
            strcpy(information, "【失败】上传失败");
            updateTextBrowser(information, -1);
            ui->progressBar->setValue(100);
            return;
        }
        updateTextBrowser(sentence, 2);
    } else if (ui->radioButtonPasv->isChecked()) {
        // 被动模式
        sprintf(information, "【信息】以被动模式上传 [%s]...", ch);
        updateTextBrowser(information, 1);
        ui->progressBar->reset();
        // 调用 PASV 命令
        if (PASV() != 0) {
            updateTextBrowser(information, -1);
            strcpy(information, "【失败】上传失败");
            updateTextBrowser(information, -1);
            ui->progressBar->setValue(100);
            return;
        }
        updateTextBrowser(sentence, 2);
    }
    // 调用 STOR 命令
    if (STOR(ch) != 0) {
        updateTextBrowser(information, -1);
        strcpy(information, "【失败】上传失败");
        updateTextBrowser(information, -1);
        ui->progressBar->setValue(100);
        return;
    }
    // 上传成功
    updateTextBrowser(sentence, 2);
    strcpy(information, "【成功】上传成功");
    updateTextBrowser(information, 0);
    ui->progressBar->setValue(100);
    return;
}

/*********************
函数: 下载函数
********************/
void Widget::downloading() {
    // 登录状态判断
    if (!isConnect || !isLogin) {
        strcpy(information, "【提示】请先连接到服务器并登录");
        updateTextBrowser(information, 1);
        return;
    }
    // 目标获取
    QString fileName = QFileDialog::getSaveFileName(this, tr("下载文件"), ".");
    if (fileName == "") {
        strcpy(information, "【提示】请选择有效文件");
        updateTextBrowser(information, 1);
        return;
    }
    QByteArray ba_ = fileName.toLatin1();
    char *ch_ = ba_.data();
    QByteArray ba = ui->lineEdit_4->text().toLatin1();
    char *ch = ba.data();
    // 判断模式
    if (ui->radioButtonPort->isChecked()) {
        // 主动模式
        sprintf(information, "【信息】以主动模式下载 [%s]...", ch);
        updateTextBrowser(information, 1);
        ui->progressBar->reset();
        // 获取 IP 与 Port
        string strIP = ui->lineEditIP_2->text().toStdString();
        string strPort = ui->lineEditPort_2->text().toStdString();
        if (inet_addr(strIP.c_str()) == INADDR_NONE) {
            // IP 格式错误
            strcpy(information, "【错误】IP 格式错误");
            updateTextBrowser(information, -1);
            ui->progressBar->setValue(100);
            return;
        } else if (atoi(strPort.c_str()) <= 0) {
            // port 格式错误
            strcpy(information, "【错误】Port 格式错误");
            updateTextBrowser(information, -1);
            ui->progressBar->setValue(100);
            return;
        }
        // 调用 PORT 命令
        unsigned int IP = inet_addr(strIP.c_str());
        unsigned int port = static_cast<unsigned int>(atoi(strPort.c_str()));
        char str[32];
        if (formatIPandPort(str, IP, port) == -1) {
            strcpy(information, "【错误】IP/Port 格式错误");
            updateTextBrowser(information, -1);
            ui->progressBar->setValue(100);
            return;
        }
        if (PORT(str) != 0) {
            updateTextBrowser(information, -1);
            strcpy(information, "【失败】下载失败");
            updateTextBrowser(information, -1);
            ui->progressBar->setValue(100);
            return;
        }
        updateTextBrowser(sentence, 2);
    } else if (ui->radioButtonPasv->isChecked()) {
        // 被动模式
        sprintf(information, "【信息】以被动模式下载 [%s]...", ch);
        updateTextBrowser(information, 1);
        ui->progressBar->reset();
        // 调用 PASV 命令
        if (PASV() != 0) {
            updateTextBrowser(information, -1);
            strcpy(information, "【失败】下载失败");
            updateTextBrowser(information, -1);
            ui->progressBar->setValue(100);
            return;
        }
        updateTextBrowser(sentence, 2);
    }
    // 调用 RETR 命令
    if (RETR(ch, ch_) != 0) {
        updateTextBrowser(information, -1);
        strcpy(information, "【失败】下载失败");
        updateTextBrowser(information, -1);
        ui->progressBar->setValue(100);
        return;
    }
    // 下载成功
    updateTextBrowser(sentence, 2);
    strcpy(information, "【成功】下载成功");
    updateTextBrowser(information, 0);
    ui->progressBar->setValue(100);
    return;
}




void Widget::clickTableItem(QTableWidgetItem *item) {
    int r = item->row();
    QTableWidgetItem *nameItem = ui->tableWidget->item(r, 0);
    ui->lineEdit_2->setText(nameItem->text());
    ui->lineEdit_4->setText(nameItem->text());
    ui->lineEdit_5->setText(nameItem->text());
    return;
}


void Widget::doubleclickTableItem(QTableWidgetItem  *item)
{
    int r = item->row();
    QTableWidgetItem *typeItem = ui->tableWidget->item(r, 1);
    QTableWidgetItem *nameItem = ui->tableWidget->item(r, 0);
    if ((typeItem->text() != "文件夹") && (nameItem->text() != ".."))  {
        strcpy(information, "【提示】请双击文件夹切换目录");
        updateTextBrowser(information, 1);
        return;
    }
    char*  ch;
    QByteArray ba = nameItem->text().toLatin1();
    ch = ba.data();
    changing(ch);
    return;
}
