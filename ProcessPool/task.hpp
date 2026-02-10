#include<iostream>
#include<vector>
#include<string>
#include<unistd.h>
#include<sys/wait.h>
#include <signal.h>
#include <cstring>
#include <cstdlib>

typedef void(*work_t)();
typedef void (*task_t)(); 
using namespace std;
//具体的任务
void PrintLog()
{
    cout << "[" << getpid() << "] Execute Task: Print log" << endl;
}

void ReloadConf()
{
    cout << "[" << getpid() << "] Execute Task: Reload config" << endl;
}

void ConnectMysql()
{
    cout << "[" << getpid() << "] Execute Task: Connect mysql" << endl;
}

task_t tasks[3] = {PrintLog, ReloadConf, ConnectMysql};

uint32_t NextTask()
{
    return rand() % 3;
}

void worker()
{
    // 从0中读取任务即可！
    while(true)
    {
        uint32_t command_code = 0;
        ssize_t n = read(0, &command_code, sizeof(command_code));
        if(n == sizeof(command_code))
        {
            if(command_code >= 3) continue;
            tasks[command_code]();
        }
        else{
            cout<<"read nothing"<<n<<endl;
        }
        cout << "I am worker: " << getpid() << endl;
        sleep(10);
    }
}
