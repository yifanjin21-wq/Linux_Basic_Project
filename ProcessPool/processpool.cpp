//创建一个进程池，父进程依次创建多个子进程执行任务
//利用dup2重定向

#include"task.hpp"

using namespace std;
enum{
    UsageError=1,
    ArgError,
    PipeError,
    ForkError
};

task_t task[3]={PrintLog,ReloadConf,ConnectMysql};
void Usage(const string &proc)
{
    cout << "Usage: " << proc << " subprocess-num" << endl;
}

class Channel
{
public:
    Channel(int wfd, pid_t sub_id, const std::string &name)
        : _wfd(wfd), _sub_process_id(sub_id), _name(name)
    {
    }
    void PrintDebug()
    {
        cout << "[Channel Debug] "
             << "wfd: " << _wfd
             << ", pid: " << _sub_process_id
             << ", name: " << _name << endl;
    }

    string name() const { return _name; }
    int wfd() const { return _wfd; }
    pid_t pid() const { return _sub_process_id; }
    void CloseWriteFd()
    {
        if (_wfd >= 0)
        {
            close(_wfd);
            _wfd = -1;
        }
    }
    
    ~Channel()
    {}
private:
    int _wfd;          // 父进程写端fd
    pid_t _sub_process_id; // 子进程PID
    string _name;      // 信道名称

};
class ProcessPool
{
public:
    ProcessPool(int sub_process_num) 
        : _sub_process_num(sub_process_num), _next_channel(0)
    {
        // 忽略SIGCHLD信号，避免子进程变成僵尸进程（也可主动waitpid）
        signal(SIGCHLD, SIG_IGN);
    }


    int CreateProcess(work_t work)
    {
        if(work==nullptr)
        {
            cerr<<"Error work function is null"<<endl;
            return -1;
        }

        for(int number=0;number<_sub_process_num;number++)
        {
            int pipefd[2]{0};
            if(pipe(pipefd)<0)
            {
                cerr << "Pipe create failed, errno: " << errno << ", msg: " << strerror(errno) << endl;
                return PipeError;
            }
            pid_t id = fork();
            if (id < 0)
            {
                cerr << "Fork failed, errno: " << errno << ", msg: " << strerror(errno) << endl;
                close(pipefd[0]);
                close(pipefd[1]);
                return ForkError;
            }
            else if(id==0)
            {
                cout<<"new Process"<<endl;
                close(pipefd[1]); // 关当前管道的写端
            // 新增：校验dup2是否成功（必须加！）
                if (dup2(pipefd[0], STDIN_FILENO) < 0) {
                    cerr << "dup2 failed, pid: " << getpid() << endl;
                    exit(1);
                    }
                // close(pipefd[0]);
                worker();
                exit(0);
            }
            string cname = "channel-" + to_string(number);
            // father
            close(pipefd[0]);
            channels.push_back(Channel(pipefd[1], id, cname));//传入写端
            sleep(1);
        }
        cout << "Success create " << _sub_process_num << " sub processes!" << endl;
        return 0;
    }
    int NextChannel()
    {
        static int next = 0;
        int c = next;
        next++;
        next %= channels.size();
        return c;
    }
    void SendTaskCode(int index, uint32_t code)
    {
        cout << "send code: " << code << " to " << channels[index].name() << " sub prorcess id: " <<  channels[index].pid() << endl;
        write(channels[index].wfd(), &code, sizeof(code));
    }
    void Debug()
    {
        for (auto &channel : channels)
        {
            channel.PrintDebug();
        }
    }
    void RecycleAll()
    {
        cout << "\nRecycle all sub processes..." << endl;
        // 关闭所有写端，让子进程读到EOF后退出
        for (auto &ch : channels)
        {
            ch.CloseWriteFd();
        }

        // 等待所有子进程退出
        for (const auto &ch : channels)
        {
            waitpid(ch.pid(), nullptr, 0);
            cout << "Sub process " << ch.pid() << " exited." << endl;
        }
        channels.clear();
    }
    ~ProcessPool()
    {
        RecycleAll();
    }
private:
    int _sub_process_num;    // 子进程数量
    vector<Channel> channels;// 信道列表（父进程写端+子进程信息）
    int _next_channel;       // 下一个要使用的信道索引
};
int main(int argc,char *argv[])
{
    if(argc!=2)
    {
        Usage(argv[0]);
        return UsageError;
    }
    int sub_process_num = std::stoi(argv[1]);
    if (sub_process_num <= 0)
        return ArgError;
    srand((uint64_t)time(nullptr));
    ProcessPool *processpool_ptr = new ProcessPool(sub_process_num);
    processpool_ptr->CreateProcess(worker);
    while(true)
    {
        // a. 选择一个进程和通道
        int channel = processpool_ptr->NextChannel();
        // cout << channel.name() << endl;

        // b. 你要选择一个任务
        uint32_t code = NextTask();

        // c. 发送任务
        processpool_ptr->SendTaskCode(channel, code);

        sleep(5);
    }

    delete processpool_ptr;
    cout << "Main process exit successfully." << endl;
    return 0;
}