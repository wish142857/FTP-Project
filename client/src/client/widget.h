#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QTableWidgetItem>
#include <QMessageBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QProgressBar>
#include <string>

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

    public:
        Widget(QWidget *parent = nullptr);
        ~Widget();
    private:
        Ui::Widget *ui;
        int isConnect;           // 是否已连接
        int isLogin;             // 是否已登录
        void refresh();          // 刷新函数
        void updateTextBrowser(char *text, int type);   // 更新文本框
        void updateTable();                             // 更新表格
        void clearTable();                              // 清空列表
        void addTableItem(std::string filename, std::string filetype,
                          std::string filesize, std::string filetime);      // 添加表格元素
    private slots:
        void connecting(); // 开始连接
        void logining();   // 开始登录
        void making();    // 创建目录
        void removing();   // 删除目录
        void changing(char *filename);   // 切换目录
        void renaming();      // 重命名目录
        void uploading();     // 进行上传
        void downloading();   // 进行下载

        void clickTableItem(QTableWidgetItem *item);
        void doubleclickTableItem(QTableWidgetItem *item);
};
#endif // WIDGET_H
