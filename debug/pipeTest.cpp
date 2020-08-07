// sampleAlg: BOJ18870
// [Input]
// 5
// 2 4 -10 4 -9
// [Output]
// 2 3 0 3 1

#include <iostream>
#include <string>
#include <chrono>
#include <filesystem>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

namespace fs = std::filesystem;
using sc = std::chrono::steady_clock;

const int FROM_PARENT = 0;
const int FROM_CHILD = 1;
const int OUTPUT = 0;
const int INPUT = 1;
const std::string PATH = fs::canonical("./sampleAlg");
const char SAMPLE_INPUT[] = "5\n2 4 -10 4 -9\n";
const int TIMEOUT = 1;

int main()
{
    int fd[2][2];

    if (pipe(fd[FROM_PARENT]) != 0 || pipe(fd[FROM_CHILD]) != 0)
    {
        std::cout << "Can't create pipe...\n";
        return -1;
    }

    if (fcntl(fd[FROM_CHILD][OUTPUT], F_SETFL, O_NONBLOCK) < 0)
    {
        std::cout << "Can't use non-blocking pipe...\n";
        return -1;
    }

    sigset_t mask, oldMask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &mask, &oldMask) < 0)
    {
        std::cout << "Can't block SIGCHLD...\n";
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        std::cout << "Can't fork...\n";
        return -1;
    }

    if (pid == 0) // Child
    {
        dup2(fd[FROM_PARENT][OUTPUT], STDIN_FILENO);
        dup2(fd[FROM_CHILD][INPUT], STDOUT_FILENO);

        prctl(PR_SET_PDEATHSIG, SIGTERM);
        execl(PATH.c_str(), PATH.c_str(), NULL);
    }

    // Parent
    write(fd[FROM_PARENT][INPUT], SAMPLE_INPUT, sizeof(SAMPLE_INPUT));
    std::cout << "Waiting for the child to terminate...\n";
    sc::time_point start = sc::now();
    timespec timeout;
    timeout.tv_sec = TIMEOUT;
    timeout.tv_nsec = 0;

    while (true)
    {
        if (sigtimedwait(&mask, NULL, &timeout) < 0)
        {
            if (errno == EINTR)
                continue;
            else if (errno == EAGAIN)
            {
                std::cout << "Timeout!\n";
                kill(pid, SIGKILL);
                break;
            }
            else
            {
                std::cout << "Unexpected error at sigtimedwait... errno was " << errno << "\n";
                return -1;
            }
        }

        break;
    }

    int exeRet;
    waitpid(pid, &exeRet, 0);
    sc::time_point end = sc::now();
    auto t = end - start;
    long long elapsedMillisec = t.count() / 1000000;
    
    std::cout << "The process has been terminated.\n";
    std::cout << "Exit value: " << exeRet << "\n";
    std::cout << "Elapsed: " << elapsedMillisec << "ms\n";
    std::cout << "[Output]\n";

    char buffer[4096];

    while (true)
    {
        int length = read(fd[FROM_CHILD][OUTPUT], buffer, sizeof(buffer));
        if (length == -1)
        {
            int e = errno;

            if (e == EAGAIN)
            {
                std::cout << "(End of output)\n";
                break;
            }
            else
            {
                std::cout << "Unexpected error while reading... errno was " << errno << "\n";
                return -1;
            }
        }
        else if (length == 0)
        {
            std::cout << "(End of output)\n";
            break;
        }
        else
        {
            std::string part;
            for (int i = 0; i < length; ++i)
                part += buffer[i];
            std::cout << part;
        }
    }

    close(fd[FROM_PARENT][INPUT]);
    close(fd[FROM_PARENT][OUTPUT]);
    close(fd[FROM_CHILD][INPUT]);
    close(fd[FROM_CHILD][OUTPUT]);

    int state;
}