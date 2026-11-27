#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <limits.h>
#include <regex>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/syscall.h>
#include <cerrno>

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;
    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h

SmallShell::SmallShell() : prompt("smash"), last_pwd(nullptr), pending_timeout_duration(0), curr_fg_pid(-1) {
    // TODO: add your implementation
}

SmallShell::~SmallShell() {
    delete[] last_pwd;
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    string cmd_s = _trim(string(cmd_line));

    string normalized = cmd_s;
    if (!normalized.empty() && normalized.back() == '&') {
        normalized.pop_back();
        normalized = _trim(normalized);
    }
    string firstWord = normalized.substr(0, normalized.find_first_of(WHITESPACE));

    bool in_quotes = false;
    bool has_pipe = false;
    bool has_redir = false;
    for (char c : cmd_s) {
        if (c == '\'') {
            in_quotes = !in_quotes;
        } else if (!in_quotes) {
            if (c == '|') has_pipe = true;
            else if (c == '>') has_redir = true;
        }
    }

    if (has_pipe)
        return new PipeCommand(cmd_line);
    else if (has_redir)
        return new RedirectionCommand(cmd_line);

    if (firstWord == "chprompt")
        return new ChPromptCommand(cmd_line);
    else if (firstWord == "pwd" || firstWord == "pwd&")
        return new GetCurrDirCommand(cmd_line);
    else if (firstWord == "showpid" || firstWord == "showpid&")
        return new ShowPidCommand(cmd_line);
    else if (firstWord == "cd" || firstWord == "cd&")
        return new ChangeDirCommand(cmd_line, &last_pwd);
    else if (firstWord == "jobs")
        return new JobsCommand(cmd_line, &jobs);
    else if (firstWord == "fg")
        return new ForegroundCommand(cmd_line, &jobs);
    else if (firstWord == "kill")
        return new KillCommand(cmd_line, &jobs);
    else if (firstWord == "quit")
        return new QuitCommand(cmd_line, &jobs);
    else if (firstWord == "alias" || firstWord == "alias&")
        return new AliasCommand(cmd_line);
    else if (firstWord == "unalias" || firstWord == "unalias&")
        return new UnAliasCommand(cmd_line);
    else if (firstWord == "unsetenv" || firstWord == "unsetenv&")
        return new UnSetEnvCommand(cmd_line);
    else if (firstWord == "getfiletype" || firstWord == "getfiletype&")
        return new GetFileTypeCommand(cmd_line);
    else if (firstWord == "du" || firstWord == "du&")
        return new DiskUsageCommand(cmd_line);
    else if (firstWord == "whoami" || firstWord == "whoami&")
        return new WhoAmICommand(cmd_line);
    else if (firstWord == "timeout" || firstWord == "timeout&")
        return new TimeoutCommand(cmd_line);
    else {
        return new ExternalCommand(cmd_line);
    }
    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    string cmd_s = _trim(string(cmd_line));

    if (cmd_s.empty()) {
        return;
    }

    string clean_orig(cmd_line);
    while (!clean_orig.empty() && (clean_orig.back() == '\r' || clean_orig.back() == '\n')) {
        clean_orig.pop_back();
    }
    original_cmd_line = clean_orig;
    string expanded_cmd = expandAlias(cmd_s);
    if (_trim(expanded_cmd).empty()) {
        return;
    }

    jobs.removeFinishedJobs();
    Command* cmd = CreateCommand(expanded_cmd.c_str());
    if (cmd != nullptr) {
        cmd->execute();
        delete cmd;
    }
}

Command::Command(const char *cmd_line) : cmd_line(cmd_line) {
}

Command::~Command() {
}

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line) {
}

ChPromptCommand::ChPromptCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
}

void ChPromptCommand::execute() {
    char cmd_copy[COMMAND_MAX_LENGTH + 1];
    strcpy(cmd_copy, cmd_line.c_str());
    _removeBackgroundSign(cmd_copy);

    char *args[COMMAND_MAX_ARGS + 1];
    int num_args = _parseCommandLine(cmd_copy, args);

    if (num_args <= 1) {
        SmallShell::getInstance().setPrompt("smash");
    } else {
        SmallShell::getInstance().setPrompt(args[1]);
    }

    for (int i = 0; i < num_args; ++i) {
        free(args[i]);
    }
}

ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
}

void ShowPidCommand::execute() {
    cout << "smash pid is " << getpid() << endl;
}

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
}

void GetCurrDirCommand::execute() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        perror("smash error: getcwd failed");
        return;
    }
    cout << cwd << endl;
}

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, char **plastPwd)
    : BuiltInCommand(cmd_line), plastPwd(plastPwd) {
}

void ChangeDirCommand::execute() {
    char cmd_copy[COMMAND_MAX_LENGTH + 1];
    strcpy(cmd_copy, cmd_line.c_str());
    _removeBackgroundSign(cmd_copy);

    char *args[COMMAND_MAX_ARGS + 1];
    int num_args = _parseCommandLine(cmd_copy, args);

    if (num_args <= 1) {
        for (int i = 0; i < num_args; ++i) {
            free(args[i]);
        }
        return;
    }

    if (num_args > 2) {
        cerr << "smash error: cd: too many arguments" << endl;
        for (int i = 0; i < num_args; ++i) {
            free(args[i]);
        }
        return;
    }

    char curr_dir[PATH_MAX];
    if (getcwd(curr_dir, sizeof(curr_dir)) == nullptr) {
        perror("smash error: getcwd failed");
        for (int i = 0; i < num_args; ++i) {
            free(args[i]);
        }
        return;
    }

    if (strcmp(args[1], "-") == 0) {
        if (plastPwd == nullptr || *plastPwd == nullptr) {
            cerr << "smash error: cd: OLDPWD not set" << endl;
            for (int i = 0; i < num_args; ++i) {
                free(args[i]);
            }
            return;
        }
        if (chdir(*plastPwd) != 0) {
            perror("smash error: chdir failed");
            for (int i = 0; i < num_args; ++i) {
                free(args[i]);
            }
            return;
        }
        delete[] *plastPwd;
        *plastPwd = new char[strlen(curr_dir) + 1];
        strcpy(*plastPwd, curr_dir);
    } else {
        if (chdir(args[1]) != 0) {
            perror("smash error: chdir failed");
            for (int i = 0; i < num_args; ++i) {
                free(args[i]);
            }
            return;
        }
        if (plastPwd != nullptr) {
            if (*plastPwd != nullptr) {
                delete[] *plastPwd;
            }
            *plastPwd = new char[strlen(curr_dir) + 1];
            strcpy(*plastPwd, curr_dir);
        }
    }

    for (int i = 0; i < num_args; ++i) {
        free(args[i]);
    }
}

AliasCommand::AliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
}

void AliasCommand::execute() {
    char cmd_copy[COMMAND_MAX_LENGTH + 1];
    strcpy(cmd_copy, cmd_line.c_str());
    _removeBackgroundSign(cmd_copy);
    string stripped = _trim(string(cmd_copy));

    if (stripped == "alias") {
        SmallShell::getInstance().printAliases();
        return;
    }

    std::regex pattern("^alias [a-zA-Z0-9_]+='[^']*'$");
    if (!std::regex_match(stripped, pattern)) {
        cerr << "smash error: alias: invalid alias format" << endl;
        return;
    }

    size_t eq_pos = stripped.find('=');
    string name = stripped.substr(6, eq_pos - 6);
    string command = stripped.substr(eq_pos + 2, stripped.length() - (eq_pos + 2) - 1);

    SmallShell &smash = SmallShell::getInstance();
    if (smash.isReservedCommand(name) || smash.aliasExists(name)) {
        cerr << "smash error: alias: " << name << " already exists or is a reserved command" << endl;
        return;
    }

    smash.addAlias(name, command);
}

static int checkEnvVarInProc(const char *var_name) {
    string path = "/proc/" + to_string(getpid()) + "/environ";
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("smash error: open failed");
        return -1;
    }

    string content;
    char buf[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buf, sizeof(buf))) > 0) {
        content.append(buf, bytes_read);
    }
    if (bytes_read < 0) {
        perror("smash error: read failed");
        close(fd);
        return -1;
    }
    if (close(fd) < 0) {
        perror("smash error: close failed");
        return -1;
    }

    size_t var_len = strlen(var_name);
    size_t pos = 0;
    while (pos < content.size()) {
        size_t nul_pos = content.find('\0', pos);
        if (nul_pos == string::npos) {
            nul_pos = content.size();
        }
        size_t entry_len = nul_pos - pos;
        if (entry_len > var_len &&
            content.compare(pos, var_len, var_name) == 0 &&
            content[pos + var_len] == '=') {
            return 1;
        }
        pos = nul_pos + 1;
    }
    return 0;
}

UnSetEnvCommand::UnSetEnvCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
}

void UnSetEnvCommand::execute() {
    char cmd_copy[COMMAND_MAX_LENGTH + 1];
    strcpy(cmd_copy, cmd_line.c_str());
    _removeBackgroundSign(cmd_copy);

    char *args[COMMAND_MAX_ARGS + 1];
    int num_args = _parseCommandLine(cmd_copy, args);

    if (num_args <= 1) {
        cerr << "smash error: unsetenv: not enough arguments" << endl;
        for (int i = 0; i < num_args; ++i) {
            free(args[i]);
        }
        return;
    }

    for (int i = 1; i < num_args; ++i) {
        int status = checkEnvVarInProc(args[i]);
        if (status == -1) {
            break;
        }
        if (status == 0) {
            cerr << "smash error: unsetenv: " << args[i] << " does not exist" << endl;
            break;
        }

        size_t var_len = strlen(args[i]);
        for (int j = 0; __environ[j] != nullptr; ++j) {
            if (strncmp(__environ[j], args[i], var_len) == 0 && __environ[j][var_len] == '=') {
                for (int k = j; __environ[k] != nullptr; ++k) {
                    __environ[k] = __environ[k + 1];
                }
                break;
            }
        }
    }

    for (int i = 0; i < num_args; ++i) {
        free(args[i]);
    }
}

UnAliasCommand::UnAliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
}

void UnAliasCommand::execute() {
    char cmd_copy[COMMAND_MAX_LENGTH + 1];
    strcpy(cmd_copy, cmd_line.c_str());
    _removeBackgroundSign(cmd_copy);

    char *args[COMMAND_MAX_ARGS + 1];
    int num_args = _parseCommandLine(cmd_copy, args);

    if (num_args <= 1) {
        cerr << "smash error: unalias: not enough arguments" << endl;
        for (int i = 0; i < num_args; ++i) {
            free(args[i]);
        }
        return;
    }

    SmallShell &smash = SmallShell::getInstance();
    for (int i = 1; i < num_args; ++i) {
        if (!smash.removeAlias(args[i])) {
            cerr << "smash error: unalias: " << args[i] << " alias does not exist" << endl;
            break;
        }
    }

    for (int i = 0; i < num_args; ++i) {
        free(args[i]);
    }
}

GetFileTypeCommand::GetFileTypeCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
}

void GetFileTypeCommand::execute() {
    char cmd_copy[COMMAND_MAX_LENGTH + 1];
    strcpy(cmd_copy, cmd_line.c_str());
    _removeBackgroundSign(cmd_copy);

    char *args[COMMAND_MAX_ARGS + 1];
    int num_args = _parseCommandLine(cmd_copy, args);

    if (num_args != 2) {
        cerr << "smash error: getfiletype: invalid arguments" << endl;
        for (int i = 0; i < num_args; ++i) {
            free(args[i]);
        }
        return;
    }

    struct stat st;
    if (lstat(args[1], &st) == -1) {
        perror("smash error: lstat failed");
        for (int i = 0; i < num_args; ++i) {
            free(args[i]);
        }
        return;
    }

    string file_type;
    if (S_ISREG(st.st_mode)) {
        file_type = "regular file";
    } else if (S_ISDIR(st.st_mode)) {
        file_type = "directory";
    } else if (S_ISCHR(st.st_mode)) {
        file_type = "character device";
    } else if (S_ISBLK(st.st_mode)) {
        file_type = "block device";
    } else if (S_ISFIFO(st.st_mode)) {
        file_type = "FIFO";
    } else if (S_ISLNK(st.st_mode)) {
        file_type = "symbolic link";
    } else if (S_ISSOCK(st.st_mode)) {
        file_type = "socket";
    }

    cout << args[1] << "'s type is \"" << file_type << "\" and takes up " << st.st_size << " bytes" << endl;

    for (int i = 0; i < num_args; ++i) {
        free(args[i]);
    }
}

struct linux_dirent64 {
    ino64_t        d_ino;
    off64_t        d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[1];
};

struct FileId {
    dev_t dev;
    ino_t ino;
};

static bool calculateDiskUsage(const string &path, long long &total_bytes, vector<FileId> &visited) {
    struct stat st;
    if (lstat(path.c_str(), &st) == -1) {
        perror("smash error: lstat failed");
        return false;
    }

    if (!S_ISDIR(st.st_mode) && st.st_nlink > 1) {
        for (const auto &entry : visited) {
            if (entry.dev == st.st_dev && entry.ino == st.st_ino) {
                return true;
            }
        }
        visited.push_back({st.st_dev, st.st_ino});
    }

    total_bytes += static_cast<long long>(st.st_blocks) * 512;

    if (S_ISDIR(st.st_mode)) {
        int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
        if (fd == -1) {
            perror("smash error: open failed");
            return false;
        }

        char buf[4096];
        ssize_t nread;
        while ((nread = syscall(SYS_getdents64, fd, buf, sizeof(buf))) > 0) {
            for (ssize_t bpos = 0; bpos < nread;) {
                struct linux_dirent64 *d = (struct linux_dirent64 *)(buf + bpos);
                if (strcmp(d->d_name, ".") != 0 && strcmp(d->d_name, "..") != 0) {
                    string child_path = (path.back() == '/') ? (path + d->d_name) : (path + "/" + d->d_name);
                    if (!calculateDiskUsage(child_path, total_bytes, visited)) {
                        close(fd);
                        return false;
                    }
                }
                bpos += d->d_reclen;
            }
        }

        if (nread == -1) {
            perror("smash error: getdents64 failed");
            close(fd);
            return false;
        }

        if (close(fd) == -1) {
            perror("smash error: close failed");
            return false;
        }
    }

    return true;
}

DiskUsageCommand::DiskUsageCommand(const char *cmd_line) : Command(cmd_line) {
}

void DiskUsageCommand::execute() {
    char cmd_copy[COMMAND_MAX_LENGTH + 1];
    strcpy(cmd_copy, cmd_line.c_str());
    _removeBackgroundSign(cmd_copy);

    char *args[COMMAND_MAX_ARGS + 1];
    int num_args = _parseCommandLine(cmd_copy, args);

    if (num_args > 2) {
        cerr << "smash error: du: too many arguments" << endl;
        for (int i = 0; i < num_args; ++i) {
            free(args[i]);
        }
        return;
    }

    string target_path;
    if (num_args == 1) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == nullptr) {
            perror("smash error: getcwd failed");
            for (int i = 0; i < num_args; ++i) {
                free(args[i]);
            }
            return;
        }
        target_path = cwd;
    } else {
        target_path = args[1];
    }

    long long total_bytes = 0;
    vector<FileId> visited;
    if (calculateDiskUsage(target_path, total_bytes, visited)) {
        long long total_kb = (total_bytes + 1023) / 1024;
        cout << "Total disk usage: " << total_kb << " KB" << endl;
    }

    for (int i = 0; i < num_args; ++i) {
        free(args[i]);
    }
}

WhoAmICommand::WhoAmICommand(const char *cmd_line) : Command(cmd_line) {
}

void WhoAmICommand::execute() {
    uid_t uid = getuid();
    gid_t gid = getgid();

    int fd = open("/etc/passwd", O_RDONLY);
    if (fd == -1) {
        perror("smash error: open failed");
        return;
    }

    string content;
    char buf[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buf, sizeof(buf))) > 0) {
        content.append(buf, bytes_read);
    }
    if (bytes_read == -1) {
        perror("smash error: read failed");
        close(fd);
        return;
    }
    if (close(fd) == -1) {
        perror("smash error: close failed");
        return;
    }

    string uid_str = to_string(uid);
    size_t line_start = 0;
    while (line_start < content.size()) {
        size_t line_end = content.find('\n', line_start);
        if (line_end == string::npos) {
            line_end = content.size();
        }
        string line = content.substr(line_start, line_end - line_start);
        line_start = line_end + 1;

        if (line.empty() || line[0] == '#') {
            continue;
        }

        vector<string> fields;
        size_t field_start = 0;
        while (true) {
            size_t colon = line.find(':', field_start);
            if (colon == string::npos) {
                fields.push_back(line.substr(field_start));
                break;
            }
            fields.push_back(line.substr(field_start, colon - field_start));
            field_start = colon + 1;
        }

        if (fields.size() >= 6 && fields[2] == uid_str) {
            cout << fields[0] << endl;
            cout << uid << endl;
            cout << gid << endl;
            cout << fields[5] << endl;
            return;
        }
    }
}

TimeoutCommand::TimeoutCommand(const char *cmd_line) : Command(cmd_line) {
}

void TimeoutCommand::execute() {
    string trimmed = _trim(cmd_line);
    size_t first_space = trimmed.find_first_of(WHITESPACE);
    if (first_space == string::npos) {
        cerr << "smash error: timeout: invalid arguments" << endl;
        return;
    }

    size_t dur_start = trimmed.find_first_not_of(WHITESPACE, first_space);
    if (dur_start == string::npos) {
        cerr << "smash error: timeout: invalid arguments" << endl;
        return;
    }

    size_t dur_end = trimmed.find_first_of(WHITESPACE, dur_start);
    if (dur_end == string::npos) {
        cerr << "smash error: timeout: invalid arguments" << endl;
        return;
    }

    string dur_str = trimmed.substr(dur_start, dur_end - dur_start);
    for (char c : dur_str) {
        if (!isdigit(static_cast<unsigned char>(c))) {
            cerr << "smash error: timeout: invalid arguments" << endl;
            return;
        }
    }

    long long duration = 0;
    try {
        duration = stoll(dur_str);
    } catch (...) {
        cerr << "smash error: timeout: invalid arguments" << endl;
        return;
    }
    if (duration <= 0 || duration > INT_MAX) {
        cerr << "smash error: timeout: invalid arguments" << endl;
        return;
    }

    size_t cmd_start = trimmed.find_first_not_of(WHITESPACE, dur_end);
    if (cmd_start == string::npos) {
        cerr << "smash error: timeout: invalid arguments" << endl;
        return;
    }

    string inner_cmd = trimmed.substr(cmd_start);
    char inner_copy[COMMAND_MAX_LENGTH + 1];
    strcpy(inner_copy, inner_cmd.c_str());
    _removeBackgroundSign(inner_copy);
    if (_trim(string(inner_copy)).empty()) {
        cerr << "smash error: timeout: invalid arguments" << endl;
        return;
    }

    SmallShell &smash = SmallShell::getInstance();
    string raw_cmd = smash.getOriginalCmdLine();
    if (raw_cmd.empty()) {
        raw_cmd = cmd_line;
    }

    string expanded_inner = smash.expandAlias(inner_cmd);
    if (_trim(expanded_inner).empty()) {
        return;
    }

    smash.setPendingTimeout(static_cast<int>(duration), raw_cmd);
    Command *cmd = smash.CreateCommand(expanded_inner.c_str());
    if (cmd != nullptr) {
        cmd->execute();
        delete cmd;
    }

    if (smash.hasPendingTimeout()) {
        smash.clearPendingTimeout();
    }
}

void SmallShell::rescheduleAlarm(time_t now) {
    time_t min_expire = 0;
    bool has_active = false;
    for (const auto &entry : timed_commands) {
        if (entry.pid > 0) {
            if (!has_active || entry.expire_time < min_expire) {
                min_expire = entry.expire_time;
                has_active = true;
            }
        }
    }

    if (has_active) {
        int remaining = static_cast<int>(min_expire - now);
        if (remaining <= 0) {
            remaining = 1;
        }
        alarm(remaining);
    } else {
        alarm(0);
    }
}

void SmallShell::addTimedCommand(pid_t pid, int duration, const std::string &cmd_line, pid_t pid2) {
    sigset_t mask, old_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigprocmask(SIG_BLOCK, &mask, &old_mask);

    time_t now = time(nullptr);
    time_t expire_time = now + duration;

    bool reused = false;
    for (auto &entry : timed_commands) {
        if (entry.pid <= 0) {
            entry.pid = pid;
            entry.pid2 = pid2;
            entry.expire_time = expire_time;
            entry.cmd_line = cmd_line;
            reused = true;
            break;
        }
    }
    if (!reused) {
        timed_commands.push_back({pid, pid2, expire_time, cmd_line});
    }

    rescheduleAlarm(now);
    sigprocmask(SIG_SETMASK, &old_mask, nullptr);
}

void SmallShell::setPendingTimeout(int duration, const std::string &cmd_line) {
    pending_timeout_duration = duration;
    pending_timeout_cmd = cmd_line;
}

void SmallShell::clearPendingTimeout() {
    pending_timeout_duration = 0;
    pending_timeout_cmd.clear();
}

bool SmallShell::hasPendingTimeout() const {
    return pending_timeout_duration > 0;
}

void SmallShell::registerTimedProcess(pid_t pid, pid_t pid2) {
    if (pending_timeout_duration > 0) {
        addTimedCommand(pid, pending_timeout_duration, pending_timeout_cmd, pid2);
        clearPendingTimeout();
    }
}

void SmallShell::removeTimedProcess(pid_t pid) {
    if (pid <= 0) return;
    sigset_t mask, old_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigprocmask(SIG_BLOCK, &mask, &old_mask);

    bool changed = false;
    for (auto &entry : timed_commands) {
        if (entry.pid == pid) {
            entry.pid = -1;
            entry.pid2 = -1;
            changed = true;
        }
    }
    if (changed) {
        rescheduleAlarm(time(nullptr));
    }

    sigprocmask(SIG_SETMASK, &old_mask, nullptr);
}

void SmallShell::handleAlarm() {
    time_t now = time(nullptr);
    for (auto &entry : timed_commands) {
        if (entry.pid > 0 && entry.expire_time <= now) {
            siginfo_t infop;
            infop.si_pid = 0;
            int res = waitid(P_PID, entry.pid, &infop, WEXITED | WNOHANG | WNOWAIT);
            bool alive1 = (res == 0 && infop.si_pid == 0 && kill(entry.pid, 0) == 0);
            bool alive2 = false;
            if (entry.pid2 > 0) {
                siginfo_t infop2;
                infop2.si_pid = 0;
                int res2 = waitid(P_PID, entry.pid2, &infop2, WEXITED | WNOHANG | WNOWAIT);
                alive2 = (res2 == 0 && infop2.si_pid == 0 && kill(entry.pid2, 0) == 0);
            }
            if (alive1 || alive2) {
                if (alive1 && kill(-entry.pid, SIGKILL) == -1) {
                    kill(entry.pid, SIGKILL);
                }
                if (alive2 && kill(-entry.pid2, SIGKILL) == -1) {
                    kill(entry.pid2, SIGKILL);
                }
                const char p1[] = "smash: ";
                const char p2[] = " timed out!\n";
                if (write(STDOUT_FILENO, p1, sizeof(p1) - 1) == -1 ||
                    write(STDOUT_FILENO, entry.cmd_line.c_str(), entry.cmd_line.length()) == -1 ||
                    write(STDOUT_FILENO, p2, sizeof(p2) - 1) == -1) {
                    entry.pid = -1;
                    entry.pid2 = -1;
                    continue;
                }
            }
            entry.pid = -1;
            entry.pid2 = -1;
        }
    }

    rescheduleAlarm(now);
}

bool SmallShell::isReservedCommand(const std::string &name) const {
    return name == "chprompt" || name == "showpid" || name == "pwd" ||
           name == "cd" || name == "jobs" || name == "fg" ||
           name == "quit" || name == "kill" || name == "alias" ||
           name == "unalias" || name == "unsetenv" || name == "getfiletype" ||
           name == "du" || name == "whoami" || name == "timeout";
}

bool SmallShell::aliasExists(const std::string &name) const {
    for (const auto &entry : aliases) {
        if (entry.first == name) {
            return true;
        }
    }
    return false;
}

void SmallShell::addAlias(const std::string &name, const std::string &command) {
    aliases.push_back({name, command});
}

bool SmallShell::removeAlias(const std::string &name) {
    for (auto it = aliases.begin(); it != aliases.end(); ++it) {
        if (it->first == name) {
            aliases.erase(it);
            return true;
        }
    }
    return false;
}

void SmallShell::printAliases() const {
    for (const auto &entry : aliases) {
        cout << entry.first << "='" << entry.second << "'" << endl;
    }
}

std::string SmallShell::expandAlias(const std::string &cmd_line) const {
    string trimmed = _trim(cmd_line);
    size_t delim_pos = trimmed.find_first_of(WHITESPACE + "|>&");

    string firstWord;
    if (delim_pos == string::npos) {
        firstWord = trimmed;
    } else {
        firstWord = trimmed.substr(0, delim_pos);
    }

    for (const auto &entry : aliases) {
        if (entry.first == firstWord) {
            string rest = (delim_pos == string::npos) ? "" : trimmed.substr(delim_pos);
            return entry.second + rest;
        }
    }
    return cmd_line;
}

std::string SmallShell::getOriginalCmdLine() const {
    return original_cmd_line;
}

std::string SmallShell::getPrompt() const {
    return prompt;
}

void SmallShell::setPrompt(const std::string &new_prompt) {
    prompt = new_prompt;
}

pid_t SmallShell::getCurrFgPid() const {
    return curr_fg_pid;
}

void SmallShell::setCurrFgPid(pid_t pid) {
    curr_fg_pid = pid;
}

static bool parseIntStrict(const char *s, int &out) {
    if (s == nullptr || *s == '\0') return false;
    try {
        size_t pos = 0;
        std::string str(s);
        int val = std::stoi(str, &pos);
        if (pos != str.length()) return false;
        out = val;
        return true;
    } catch (...) {
        return false;
    }
}

JobsList::JobsList() {}

JobsList::~JobsList() {}

void JobsList::addJob(Command *cmd, bool isStopped) {
    (void)cmd;
    (void)isStopped;
}

void JobsList::addJob(const std::string &cmd_line, pid_t pid) {
    removeFinishedJobs();
    int max_id = 0;
    for (const auto &job : jobs_vec) {
        if (job.job_id > max_id) {
            max_id = job.job_id;
        }
    }
    jobs_vec.emplace_back(max_id + 1, pid, cmd_line);
}

void JobsList::removeFinishedJobs() {
    for (auto it = jobs_vec.begin(); it != jobs_vec.end(); ) {
        int status = 0;
        pid_t res = waitpid(it->pid, &status, WNOHANG);
        if (res > 0 || (res == -1 && errno == ECHILD)) {
            SmallShell::getInstance().removeTimedProcess(it->pid);
            it = jobs_vec.erase(it);
        } else {
            ++it;
        }
    }
}

void JobsList::printJobsList() {
    removeFinishedJobs();
    for (const auto &job : jobs_vec) {
        cout << "[" << job.job_id << "] " << job.cmd_line << endl;
    }
}

void JobsList::killAllJobs() {
    removeFinishedJobs();
    cout << "smash: sending SIGKILL signal to " << jobs_vec.size() << " jobs:" << endl;
    for (const auto &job : jobs_vec) {
        cout << job.pid << ": " << job.cmd_line << endl;
        if (kill(job.pid, SIGKILL) == -1) {
            perror("smash error: kill failed");
        }
    }
    jobs_vec.clear();
}

JobsList::JobEntry *JobsList::getJobById(int jobId) {
    for (auto &job : jobs_vec) {
        if (job.job_id == jobId) {
            return &job;
        }
    }
    return nullptr;
}

void JobsList::removeJobById(int jobId) {
    for (auto it = jobs_vec.begin(); it != jobs_vec.end(); ++it) {
        if (it->job_id == jobId) {
            jobs_vec.erase(it);
            return;
        }
    }
}

JobsList::JobEntry *JobsList::getLastJob(int *lastJobId) {
    if (jobs_vec.empty()) return nullptr;
    JobEntry *last = &jobs_vec.back();
    if (lastJobId != nullptr) {
        *lastJobId = last->job_id;
    }
    return last;
}

JobsList::JobEntry *JobsList::getLastStoppedJob(int *jobId) {
    (void)jobId;
    return nullptr;
}

bool JobsList::isEmpty() const {
    return jobs_vec.empty();
}

JobsCommand::JobsCommand(const char *cmd_line, JobsList *jobs)
    : BuiltInCommand(cmd_line), jobs(jobs) {}

void JobsCommand::execute() {
    jobs->printJobsList();
}

ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs)
    : BuiltInCommand(cmd_line), jobs(jobs) {}

void ForegroundCommand::execute() {
    char cmd_copy[COMMAND_MAX_LENGTH + 1];
    strcpy(cmd_copy, cmd_line.c_str());
    _removeBackgroundSign(cmd_copy);

    char *args[COMMAND_MAX_ARGS + 1];
    int num_args = _parseCommandLine(cmd_copy, args);

    JobsList::JobEntry *job = nullptr;
    int job_id = -1;

    if (num_args > 2) {
        cerr << "smash error: fg: invalid arguments" << endl;
        for (int i = 0; i < num_args; ++i) free(args[i]);
        return;
    }

    if (num_args == 1) {
        if (jobs->isEmpty()) {
            cerr << "smash error: fg: jobs list is empty" << endl;
            for (int i = 0; i < num_args; ++i) free(args[i]);
            return;
        }
        job = jobs->getLastJob(&job_id);
    } else {
        if (!parseIntStrict(args[1], job_id)) {
            cerr << "smash error: fg: invalid arguments" << endl;
            for (int i = 0; i < num_args; ++i) free(args[i]);
            return;
        }
        job = jobs->getJobById(job_id);
        if (job == nullptr) {
            cerr << "smash error: fg: job-id " << job_id << " does not exist" << endl;
            for (int i = 0; i < num_args; ++i) free(args[i]);
            return;
        }
    }

    for (int i = 0; i < num_args; ++i) free(args[i]);

    pid_t pid = job->pid;
    string job_cmd = job->cmd_line;
    cout << job_cmd << " " << pid << endl;
    jobs->removeJobById(job_id);

    SmallShell &smash = SmallShell::getInstance();
    smash.setCurrFgPid(pid);
    int status = 0;
    if (waitpid(pid, &status, 0) == -1 && errno != EINTR) {
        perror("smash error: waitpid failed");
    }
    smash.removeTimedProcess(pid);
    smash.setCurrFgPid(-1);
}

QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs)
    : BuiltInCommand(cmd_line), jobs(jobs) {}

void QuitCommand::execute() {
    char cmd_copy[COMMAND_MAX_LENGTH + 1];
    strcpy(cmd_copy, cmd_line.c_str());
    _removeBackgroundSign(cmd_copy);

    char *args[COMMAND_MAX_ARGS + 1];
    int num_args = _parseCommandLine(cmd_copy, args);

    bool do_kill = (num_args >= 2 && strcmp(args[1], "kill") == 0);
    for (int i = 0; i < num_args; ++i) free(args[i]);

    if (do_kill) {
        jobs->killAllJobs();
    }
    exit(0);
}

KillCommand::KillCommand(const char *cmd_line, JobsList *jobs)
    : BuiltInCommand(cmd_line), jobs(jobs) {}

void KillCommand::execute() {
    char cmd_copy[COMMAND_MAX_LENGTH + 1];
    strcpy(cmd_copy, cmd_line.c_str());
    _removeBackgroundSign(cmd_copy);

    char *args[COMMAND_MAX_ARGS + 1];
    int num_args = _parseCommandLine(cmd_copy, args);

    int signum = 0;
    int job_id = 0;
    if (num_args != 3 || args[1][0] != '-' ||
        !parseIntStrict(args[1] + 1, signum) || signum < 0 ||
        !parseIntStrict(args[2], job_id)) {
        cerr << "smash error: kill: invalid arguments" << endl;
        for (int i = 0; i < num_args; ++i) free(args[i]);
        return;
    }

    for (int i = 0; i < num_args; ++i) free(args[i]);

    JobsList::JobEntry *job = jobs->getJobById(job_id);
    if (job == nullptr) {
        cerr << "smash error: kill: job-id " << job_id << " does not exist" << endl;
        return;
    }

    if (kill(job->pid, signum) == -1) {
        perror("smash error: kill failed");
        return;
    }
    cout << "signal number " << signum << " was sent to pid " << job->pid << endl;
}

JobsList &SmallShell::getJobs() {
    return jobs;
}

ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line) {}

void ExternalCommand::execute() {
    bool is_bg = _isBackgroundComamnd(cmd_line.c_str());

    char cmd_copy[COMMAND_MAX_LENGTH + 1];
    strcpy(cmd_copy, cmd_line.c_str());
    if (is_bg) {
        _removeBackgroundSign(cmd_copy);
    }

    string clean_cmd = _trim(string(cmd_copy));
    if (clean_cmd.empty()) {
        return;
    }

    bool is_complex = (clean_cmd.find('*') != string::npos || clean_cmd.find('?') != string::npos);

    pid_t pid = fork();
    if (pid == -1) {
        perror("smash error: fork failed");
        return;
    }

    if (pid == 0) {
        setpgrp();
        if (is_complex) {
            char *bash_args[] = {
                const_cast<char *>("/bin/bash"),
                const_cast<char *>("-c"),
                const_cast<char *>(clean_cmd.c_str()),
                nullptr
            };
            execv("/bin/bash", bash_args);
            perror("smash error: execv failed");
            exit(1);
        } else {
            char *args[COMMAND_MAX_ARGS + 1];
            int num_args = _parseCommandLine(clean_cmd.c_str(), args);
            if (num_args == 0) {
                exit(0);
            }
            execvp(args[0], args);
            perror("smash error: execvp failed");
            for (int i = 0; i < num_args; ++i) free(args[i]);
            exit(1);
        }
    } else {
        SmallShell &smash = SmallShell::getInstance();
        smash.registerTimedProcess(pid);

        if (is_bg) {
            string job_cmd = smash.getOriginalCmdLine();
            if (job_cmd.empty()) {
                job_cmd = cmd_line;
            }
            smash.getJobs().addJob(job_cmd, pid);
        } else {
            smash.setCurrFgPid(pid);
            int status = 0;
            if (waitpid(pid, &status, 0) == -1 && errno != EINTR) {
                perror("smash error: waitpid failed");
            }
            smash.setCurrFgPid(-1);
        }
    }
}

RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line) {}

void RedirectionCommand::execute() {
    string s = cmd_line;
    bool in_quotes = false;
    size_t pos = string::npos;
    bool append = false;

    for (size_t i = 0; i < s.length(); ++i) {
        if (s[i] == '\'') {
            in_quotes = !in_quotes;
        } else if (!in_quotes && s[i] == '>') {
            pos = i;
            if (i + 1 < s.length() && s[i + 1] == '>') {
                append = true;
            }
            break;
        }
    }

    if (pos == string::npos) {
        return;
    }

    string inner_cmd = _trim(s.substr(0, pos));
    string out_file = _trim(s.substr(pos + (append ? 2 : 1)));

    char file_copy[COMMAND_MAX_LENGTH + 1];
    strcpy(file_copy, out_file.c_str());
    _removeBackgroundSign(file_copy);
    out_file = _trim(string(file_copy));

    char inner_copy[COMMAND_MAX_LENGTH + 1];
    strcpy(inner_copy, inner_cmd.c_str());
    _removeBackgroundSign(inner_copy);
    inner_cmd = _trim(string(inner_copy));

    int saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdout == -1) {
        perror("smash error: dup failed");
        return;
    }

    int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
    int fd = open(out_file.c_str(), flags, 0666);
    if (fd == -1) {
        perror("smash error: open failed");
        close(saved_stdout);
        return;
    }

    if (dup2(fd, STDOUT_FILENO) == -1) {
        perror("smash error: dup2 failed");
        close(fd);
        close(saved_stdout);
        return;
    }
    close(fd);

    SmallShell &smash = SmallShell::getInstance();
    Command *cmd = smash.CreateCommand(inner_cmd.c_str());
    if (cmd != nullptr) {
        cmd->execute();
        delete cmd;
    }

    cout.flush();
    if (dup2(saved_stdout, STDOUT_FILENO) == -1) {
        perror("smash error: dup2 failed");
    }
    close(saved_stdout);
}

PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line) {}

void PipeCommand::execute() {
    string s = cmd_line;
    bool in_quotes = false;
    size_t pos = string::npos;
    bool redirect_err = false;

    for (size_t i = 0; i < s.length(); ++i) {
        if (s[i] == '\'') {
            in_quotes = !in_quotes;
        } else if (!in_quotes && s[i] == '|') {
            pos = i;
            if (i + 1 < s.length() && s[i + 1] == '&') {
                redirect_err = true;
            }
            break;
        }
    }

    if (pos == string::npos) {
        return;
    }

    string cmd1_str = _trim(s.substr(0, pos));
    string cmd2_str = _trim(s.substr(pos + (redirect_err ? 2 : 1)));

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("smash error: pipe failed");
        return;
    }

    pid_t pid1 = fork();
    if (pid1 == -1) {
        perror("smash error: fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    if (pid1 == 0) {
        setpgrp();
        close(pipefd[0]);
        int target_fd = redirect_err ? STDERR_FILENO : STDOUT_FILENO;
        if (dup2(pipefd[1], target_fd) == -1) {
            perror("smash error: dup2 failed");
            close(pipefd[1]);
            exit(1);
        }
        close(pipefd[1]);

        SmallShell &smash = SmallShell::getInstance();
        Command *cmd1 = smash.CreateCommand(cmd1_str.c_str());
        if (cmd1 != nullptr) {
            cmd1->execute();
            delete cmd1;
        }
        cout.flush();
        cerr.flush();
        exit(0);
    }

    pid_t pid2 = fork();
    if (pid2 == -1) {
        perror("smash error: fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        waitpid(pid1, nullptr, 0);
        return;
    }

    if (pid2 == 0) {
        setpgrp();
        close(pipefd[1]);
        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            perror("smash error: dup2 failed");
            close(pipefd[0]);
            exit(1);
        }
        close(pipefd[0]);

        SmallShell &smash = SmallShell::getInstance();
        string expanded2 = smash.expandAlias(cmd2_str);
        Command *cmd2 = smash.CreateCommand(expanded2.c_str());
        if (cmd2 != nullptr) {
            cmd2->execute();
            delete cmd2;
        }
        cout.flush();
        cerr.flush();
        exit(0);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    SmallShell &smash = SmallShell::getInstance();
    smash.registerTimedProcess(pid1, pid2);

    if (waitpid(pid1, nullptr, 0) == -1 && errno != EINTR) {
        perror("smash error: waitpid failed");
    }
    if (waitpid(pid2, nullptr, 0) == -1 && errno != EINTR) {
        perror("smash error: waitpid failed");
    }
    smash.removeTimedProcess(pid1);
}