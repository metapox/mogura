#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <string.h>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <map>
#include <ctime>
#include <list>
#include <string>

struct pidctl {
  long pid;
  std::list<pidctl> child_pids;
  std::time_t start_time;
  std::time_t end_time;
  long syscall_count;
  std::string command = "";

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

void print_pidctl(long top_process, int depth = 0) {
  pidctl pc = pid_map[top_process];
  for (int i = 0; i < depth - 1; i++) {
    printf("  ");
  }

  if (depth > 0) {
    printf("└─");
  }
  printf("pid=%ld, cmd=%s, duration=%f, syscall_count=%ld\n", pc.pid, pc.command.c_str(), pc.duration(), pc.syscall_count);
  for (pidctl child_pc : pc.child_pids) {
    print_pidctl(child_pc.pid, depth + 1);
  }
}

std::string peek_and_output(pid_t pid, long long addr, long long size, int fd)
{
  std::string output = "";
  long long i;
  for (i = 0; i < size; i += sizeof(long)) {
    long data = ptrace(PTRACE_PEEKDATA, pid, addr + i, NULL);
    if (data == -1) {
      perror("failed to peek");
      exit(1);
    }
    char *c = (char *)&data;
    for (int j = 0; j < sizeof(long); j++) {
      if (c[j] == '\0') {
        return output;
      }
      output += c[j];
    }
  }
  return output;
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
  pidctl top_pidctl = {top_process, std::list<pidctl>()};
  top_pidctl.start();
  top_pidctl.command = argv[1];
  pid_map[top_process] = top_pidctl;

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
      else continue;
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
        pidctl pc = {npid, std::list<pidctl>()};
        pid_map[pid].child_pids.push_back(pc);
        pc.start();
        pid_map[npid] = pc;
        printf("fork pid=%ld\n", npid);
      } else if (WSTOPSIG(status) == (SIGTRAP | 0x80)) {
        pid_map[pid].increatement_syscall_count();
        struct user_regs_struct regs;
        ptrace(PTRACE_GETREGS, pid, NULL, &regs);
      #ifdef __x86_64__
        long syscall = regs.orig_rax;
      #else
        long syscall = regs.orig_eax;
      #endif
        if (syscall == __NR_execve && pid_map[pid].command == "") {
          pid_map[pid].command = peek_and_output(pid, regs.rdi, regs.rdx, (int)regs.rdi);
        }
      } else if (WSTOPSIG(status) == (SIGTRAP)) {
        printf("trap\n");
      } else {
        printf("stopped by signal %d\n", WSTOPSIG(status));
      }
    }

    if (ptrace(PTRACE_SYSCALL, pid, NULL, NULL) < 0) {
      printf("failed to syscall pid %ld\n", pid);
    }
  }

  print_pidctl(top_process);
  return 0;
}
