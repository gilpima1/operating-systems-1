#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    int saved_errno = errno;
    const char msg[] = "smash: got ctrl-C\n";
    if (write(STDOUT_FILENO, msg, sizeof(msg) - 1) == -1) {
        errno = saved_errno;
        return;
    }

    SmallShell &smash = SmallShell::getInstance();
    pid_t fg_pid = smash.getCurrFgPid();
    if (fg_pid > 0) {
        if (kill(-fg_pid, SIGKILL) == -1) {
            if (kill(fg_pid, SIGKILL) == -1) {
                errno = saved_errno;
                return;
            }
        }
        smash.removeTimedProcess(fg_pid);
        const char p1[] = "smash: process ";
        const char p2[] = " was killed\n";
        char num_buf[16];
        char rev[16];
        int len = 0, d = 0, tmp = fg_pid;
        while (tmp > 0) {
            rev[d++] = '0' + (tmp % 10);
            tmp /= 10;
        }
        while (d > 0) {
            num_buf[len++] = rev[--d];
        }
        if (write(STDOUT_FILENO, p1, sizeof(p1) - 1) == -1 ||
            write(STDOUT_FILENO, num_buf, len) == -1 ||
            write(STDOUT_FILENO, p2, sizeof(p2) - 1) == -1) {
            errno = saved_errno;
            return;
            }
        smash.setCurrFgPid(-1);
    }
    errno = saved_errno;
}

void alarmHandler(int sig_num) {
    int saved_errno = errno;
    const char msg[] = "smash: got an alarm\n";
    if (write(STDOUT_FILENO, msg, sizeof(msg) - 1) == -1) {
        errno = saved_errno;
        return;
    }
    SmallShell::getInstance().handleAlarm();
    errno = saved_errno;
}