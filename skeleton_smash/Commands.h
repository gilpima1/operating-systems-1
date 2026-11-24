// Ver: 20-10-2026
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string>
#include <utility>
#include <ctime>
#include <sys/types.h>

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
protected:
    std::string cmd_line;
public:
    Command(const char *cmd_line);

    virtual ~Command();

    virtual void execute() = 0;

    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char *cmd_line);

    virtual ~BuiltInCommand() {
    }
};

class ChPromptCommand : public BuiltInCommand {
public:
    ChPromptCommand(const char *cmd_line);

    virtual ~ChPromptCommand() {
    }

    void execute() override;
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char *cmd_line);

    virtual ~ExternalCommand() {
    }

    void execute() override;
};

class RedirectionCommand : public Command {
    // TODO: Add your data members
public:
    explicit RedirectionCommand(const char *cmd_line);

    virtual ~RedirectionCommand() {
    }

    void execute() override;
};

class PipeCommand : public Command {
    // TODO: Add your data members
public:
    PipeCommand(const char *cmd_line);

    virtual ~PipeCommand() {
    }

    void execute() override;
};

class DiskUsageCommand : public Command {
public:
    DiskUsageCommand(const char *cmd_line);

    virtual ~DiskUsageCommand() {
    }

    void execute() override;
};

class WhoAmICommand : public Command {
public:
    WhoAmICommand(const char *cmd_line);

    virtual ~WhoAmICommand() {
    }

    void execute() override;
};

class TimeoutCommand : public Command {
    // TODO: Add your data members **BONUS: 10 Points**
public:
    TimeoutCommand(const char *cmd_line);

    virtual ~TimeoutCommand() {
    }

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
    char **plastPwd;
public:
    ChangeDirCommand(const char *cmd_line, char **plastPwd);

    virtual ~ChangeDirCommand() {
    }

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char *cmd_line);

    virtual ~GetCurrDirCommand() {
    }

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char *cmd_line);

    virtual ~ShowPidCommand() {
    }

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
    JobsList *jobs;
public:
    QuitCommand(const char *cmd_line, JobsList *jobs);

    virtual ~QuitCommand() {
    }

    void execute() override;
};

class JobsList {
public:
    class JobEntry {
    public:
        int job_id;
        pid_t pid;
        std::string cmd_line;

        JobEntry(int job_id, pid_t pid, const std::string &cmd_line)
            : job_id(job_id), pid(pid), cmd_line(cmd_line) {}
    };

private:
    std::vector<JobEntry> jobs_vec;

public:
    JobsList();

    ~JobsList();

    void addJob(Command *cmd, bool isStopped = false);

    void addJob(const std::string &cmd_line, pid_t pid);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob(int *lastJobId);

    JobEntry *getLastStoppedJob(int *jobId);

    bool isEmpty() const;
};

class JobsCommand : public BuiltInCommand {
    JobsList *jobs;
public:
    JobsCommand(const char *cmd_line, JobsList *jobs);

    virtual ~JobsCommand() {
    }

    void execute() override;
};

class KillCommand : public BuiltInCommand {
    JobsList *jobs;
public:
    KillCommand(const char *cmd_line, JobsList *jobs);

    virtual ~KillCommand() {
    }

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    JobsList *jobs;
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs);

    virtual ~ForegroundCommand() {
    }

    void execute() override;
};

class AliasCommand : public BuiltInCommand {
public:
    AliasCommand(const char *cmd_line);

    virtual ~AliasCommand() {
    }

    void execute() override;
};

class UnAliasCommand : public BuiltInCommand {
public:
    UnAliasCommand(const char *cmd_line);

    virtual ~UnAliasCommand() {
    }

    void execute() override;
};

class UnSetEnvCommand : public BuiltInCommand {
public:
    UnSetEnvCommand(const char *cmd_line);

    virtual ~UnSetEnvCommand() {
    }

    void execute() override;
};

class GetFileTypeCommand : public BuiltInCommand {
public:
    GetFileTypeCommand(const char *cmd_line);

    virtual ~GetFileTypeCommand() {
    }

    void execute() override;
};

struct TimedCommandEntry {
    pid_t pid;
    time_t expire_time;
    std::string cmd_line;
};

class SmallShell {
private:
    std::string prompt;
    char *last_pwd;
    std::vector<std::pair<std::string, std::string>> aliases;
    std::string original_cmd_line;
    std::vector<TimedCommandEntry> timed_commands;
    int pending_timeout_duration;
    std::string pending_timeout_cmd;
    pid_t curr_fg_pid;
    JobsList jobs;
    SmallShell();

public:
    Command *CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    void executeCommand(const char *cmd_line);

    std::string getPrompt() const;

    void setPrompt(const std::string &new_prompt);

    bool isReservedCommand(const std::string &name) const;

    bool aliasExists(const std::string &name) const;

    void addAlias(const std::string &name, const std::string &command);

    bool removeAlias(const std::string &name);

    void printAliases() const;

    std::string expandAlias(const std::string &cmd_line) const;

    std::string getOriginalCmdLine() const;

    void addTimedCommand(pid_t pid, int duration, const std::string &cmd_line);

    void setPendingTimeout(int duration, const std::string &cmd_line);

    void clearPendingTimeout();

    bool hasPendingTimeout() const;

    void registerTimedProcess(pid_t pid);

    void handleAlarm();

    pid_t getCurrFgPid() const;
    void setCurrFgPid(pid_t pid);
    JobsList &getJobs();
};

#endif //SMASH_COMMAND_H_
