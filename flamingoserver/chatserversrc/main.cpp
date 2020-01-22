/**
 *  聊天服务程序入口函数
 *  zhangyl 2017.03.09
 **/
#include <iostream>
#include <stdlib.h>

#include "../base/Platform.h" //平台
#include "../base/Singleton.h"  //设计模式：单例
#include "../base/ConfigFileReader.h"
#include "../base/AsyncLog.h"  //异步log
#include "../net/EventLoop.h"
#include "../net/EventLoopThreadPool.h"
#include "../mysqlmgr/MysqlManager.h"

#ifndef WIN32 //linux环境
#include <string.h>
#include "../utils/DaemonRun.h" //工具类：守护进程运行
#endif 

#include "UserManager.h"
#include "ChatServer.h"
#include "MonitorServer.h"
#include "HttpServer.h"

using namespace net;

#ifdef WIN32
//初始化Windows socket库
NetworkInitializer windowsNetworkInitializer; //建立对象
#endif

EventLoop g_mainLoop;

#ifndef WIN32 //linux环境下
void prog_exit(int signo) //定义退出函数
{
    std::cout << "program recv signal [" << signo << "] to exit." << std::endl;

    Singleton<MonitorServer>::Instance().uninit();
        //Instance()获取MonitorServer类型的单例，并执行其uninit()
    Singleton<HttpServer>::Instance().uninit();
    Singleton<ChatServer>::Instance().uninit();
    g_mainLoop.quit();

    CAsyncLog::uninit();
}
#endif

int main(int argc, char* argv[])
{
#ifndef WIN32  //linux环境下
    //设置信号处理
    signal(SIGCHLD, SIG_DFL);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, prog_exit);
    signal(SIGTERM, prog_exit);


    int ch; //选项（应为char）
    bool bdaemon = false; //默认非守护运行
    while ((ch = getopt(argc, argv, "d")) != -1)  //遍历所有选项
    {
        switch (ch)
        {
        case 'd':
            bdaemon = true;
            break;
        }
    }

    if (bdaemon)
        daemon_run();
#endif

#ifdef WIN32
    CConfigFileReader config("../etc/chatserver.conf");
#else
    CConfigFileReader config("etc/chatserver.conf");
#endif

    const char* logbinarypackage = config.getConfigName("logbinarypackage");
    if (logbinarypackage != NULL)
    {
        int logbinarypackageint = atoi(logbinarypackage);
        if (logbinarypackageint != 0)
            Singleton<ChatServer>::Instance().enableLogPackageBinary(true);
        else
            Singleton<ChatServer>::Instance().enableLogPackageBinary(false);
    }
   
    std::string logFileFullPath;

#ifndef WIN32
    const char* logfilepath = config.getConfigName("logfiledir");
    if (logfilepath == NULL)
    {
        LOGF("logdir is not set in config file");
        return 1;
    }

    //如果log目录不存在则创建之
    DIR* dp = opendir(logfilepath);
    if (dp == NULL)
    {
        if (mkdir(logfilepath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
        {
            LOGF("create base dir error, %s , errno: %d, %s", logfilepath, errno, strerror(errno));
            return 1;
        }
    }
    closedir(dp);

    logFileFullPath = logfilepath;
#endif

    const char* logfilename = config.getConfigName("logfilename");
    logFileFullPath += logfilename;

#ifdef _DEBUG
    CAsyncLog::init();
#else
    CAsyncLog::init(logFileFullPath.c_str());
#endif
    
    //初始化数据库配置
    const char* dbserver = config.getConfigName("dbserver");
    const char* dbuser = config.getConfigName("dbuser");
    const char* dbpassword = config.getConfigName("dbpassword");
    const char* dbname = config.getConfigName("dbname");
    if (!Singleton<CMysqlManager>::Instance().init(dbserver, dbuser, dbpassword, dbname))
    {
        LOGF("Init mysql failed, please check your database config..............");
    }

    if (!Singleton<UserManager>::Instance().init(dbserver, dbuser, dbpassword, dbname))
    {
        LOGF("Init UserManager failed, please check your database config..............");
    }

    const char* listenip = config.getConfigName("listenip");
    short listenport = (short)atol(config.getConfigName("listenport"));
    Singleton<ChatServer>::Instance().init(listenip, listenport, &g_mainLoop);

    const char* monitorlistenip = config.getConfigName("monitorlistenip");
    short monitorlistenport = (short)atol(config.getConfigName("monitorlistenport"));
    const char* monitortoken = config.getConfigName("monitortoken");
    Singleton<MonitorServer>::Instance().init(monitorlistenip, monitorlistenport, &g_mainLoop, monitortoken);

    const char* httplistenip = config.getConfigName("monitorlistenip");
    short httplistenport = (short)atol(config.getConfigName("httplistenport"));
    Singleton<HttpServer>::Instance().init(httplistenip, httplistenport, &g_mainLoop);

    LOGI("chatserver initialization completed, now you can use client to connect it.");

    g_mainLoop.loop();

    LOGI("exit chatserver.");

    return 0;
}