#include "db.h"
#include <muduo/base/Logging.h>

// 数据库配置信息
static string server = "127.0.0.1";
static string user = "root";
static string password = "123456";
static string dbname = "chat";


// 初始化数据库连接
MySQL::MySQL(){
        _conn = mysql_init(nullptr);
    }

    // 释放数据库连接资源
MySQL::~MySQL(){
        if(_conn != nullptr){
            mysql_close(_conn);
        }
    }

    // 连接数据库
bool MySQL::connect()
    {
        
        MYSQL *p = mysql_real_connect(_conn,server.c_str(),user.c_str(),password.c_str(),dbname.c_str(),3306,nullptr,0);
        // C和C++代码默认的编码字符是ASCII， 如果不设置，从MYSQL上拉下来的中文将是？
        if(p != nullptr)
        {
            mysql_query(_conn,"set names gbk");
            LOG_INFO << "connect mysql success!";
        }
        else{
            LOG_INFO << "connect mysql fail!";
        }
        return p;
    }
    // 更新操作
bool MySQL::update(string sql)
    {
        if(mysql_query(_conn,sql.c_str()))
        {
            LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
                        << sql << "更新失败！";
    // 使用LOG_INFO宏（可能是一个日志库的一部分）记录错误信息  
    // __FILE__和__LINE__是C++预处理器宏，分别表示当前文件名和行号  
    // sql是执行的SQL语句，这里它被拼接到日志信息中
                        return false;
        }
        return true;
    }

    // 查询操作
MYSQL_RES* MySQL::query(string sql)
    {
        if(mysql_query(_conn, sql.c_str()))
        {
            LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
                        << sql << "查询失败!";
                        return nullptr;
        }
        return mysql_use_result(_conn);
    }


    MYSQL* MySQL::getConnection(){
        return _conn;
    }