#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <map>
#include <ctime>

struct pidctl {
  long pid;
  // top level process of parent id is -1
  pid_t parent_pid;
  std::time_t start_time;
  std::time_t end_time;
  long syscall_count;

  void start() {
    start_time = std::time(nullptr);
  }

  void end() {
    end_time = std::time(nullptr);
  }

  double duration() {
    return difftime(end_time, start_time);
  }

  long increatement_syscall_count() {
    return ++syscall_count;
  }
};
std::map <pid_t, pidctl> pid_map;

void print_pidctl() {
  for(auto itr = pid_map.begin(); itr != pid_map.end(); ++itr) {
    pidctl pidctl = itr->second;
    printf("pid=%d, parent=%d, duration=%ld\n", pidctl.pid, pidctl.parent_pid, pidctl.duration());
  }
}

int main(int argc, char *argv[])
{
  int status;

  if (argc < 2) {
    fprintf(stderr, "no command\n");
    exit(1);
  }

  long top_process = fork();
  if (top_process == 0) {
    execvp(argv[1], argv + 1);
    perror("failed to exec");
    exit(1);
  }

  printf("attach to %d\n", top_process);
  pidctl top_pidctl = {top_process, -1};
  pid_map[top_process] = top_pidctl;
  top_pidctl.start();

  if (ptrace(PTRACE_ATTACH, top_process, NULL, NULL) < 0) {
    perror("failed to attach");
    exit(1);
  }

  // wait for the process to stop
  waitpid(top_process, &status, __WALL);

  if (ptrace(PTRACE_SETOPTIONS, top_process, NULL, PTRACE_O_TRACESYSGOOD|PTRACE_O_TRACECLONE|PTRACE_O_TRACEFORK|PTRACE_O_TRACEVFORK) < 0) {
    perror("failed to set options");
    exit(1);
  }

  ptrace(PTRACE_SYSCALL, top_process, NULL, NULL);
  while (1) {
    pid_t pid = waitpid(-1, &status, __WALL);
    if (WIFEXITED(status)) {
      printf("exited, status=%d pid=%d\n", WEXITSTATUS(status) , pid);
      pid_map[pid].end();
      if (pid == top_process) break;
    } else if (WIFSIGNALED(status)) {
      printf("terminated by signal %d\n", WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
      int event = (status >> 8);
      long npid;
      if(event == (SIGTRAP | PTRACE_EVENT_VFORK << 8) ||
         event == (SIGTRAP | PTRACE_EVENT_FORK << 8) ||
         event == (SIGTRAP | PTRACE_EVENT_CLONE << 8)
        ) {
        ptrace(PTRACE_GETEVENTMSG, pid, NULL, &npid);
        pidctl pidctl = {npid, pid};
        pid_map[npid] = pidctl;
        pidctl.start();
        printf("fork pid=%ld\n", npid);
      } else if (WSTOPSIG(status) == (SIGTRAP | 0x80)) {
        pid_map[pid].increatement_syscall_count();
      } else if (WSTOPSIG(status) == (SIGTRAP)) {
        printf("trap\n");
      } else {
        printf("stopped by signal %d\n", WSTOPSIG(status));
      }
    }

    if (ptrace(PTRACE_SYSCALL, pid, NULL, NULL) < 0) {
      perror("failed to syscall");
    }
  }

  print_pidctl();
  return 0;
}


