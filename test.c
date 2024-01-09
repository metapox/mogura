#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/syscall.h>

int main(int argc, char *argv[])
{
  int status;

  if (argc < 2) {
    fprintf(stderr, "specify pid\n");
    exit(1);
  }

  pid_t pid = atoi(argv[1]);
  printf("attach to %d\n", pid);

  if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) < 0) {
    perror("failed to attach");
    exit(1);
  }

  // wait for the process to stop
  waitpid(pid, &status, __WALL);

  if (ptrace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_TRACESYSGOOD|PTRACE_O_TRACECLONE|PTRACE_O_TRACEFORK|PTRACE_O_TRACEVFORK) < 0) {
    perror("failed to set options");
    exit(1);
  }

  ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
  while (1) {
    waitpid(-1, &status, __WALL);
    if (WIFEXITED(status)) {
      printf("exited, status=%d\n", WEXITSTATUS(status));
      break;
    } else if (WIFSIGNALED(status)) {
      printf("terminated by signal %d\n", WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
      int event = (status >> 8);
      long npid;
      if(event == (SIGTRAP | PTRACE_EVENT_VFORK << 8)) {
        ptrace(PTRACE_GETEVENTMSG, pid, NULL, &npid);
        ptrace(PTRACE_SYSCALL, npid, NULL, NULL);
        ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
        printf("vfor pid=%ld\n", npid);
        continue;
      } else if (event == (SIGTRAP | PTRACE_EVENT_FORK << 8)) {
        ptrace(PTRACE_GETEVENTMSG, pid, NULL, &npid);
        ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
        ptrace(PTRACE_SYSCALL, npid, NULL, NULL);
        printf("fork pid=%ld\n", npid);
        continue;
      } else if (event == (SIGTRAP | PTRACE_EVENT_CLONE << 8)) {
        ptrace(PTRACE_GETEVENTMSG, pid, NULL, &npid);
        ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
        ptrace(PTRACE_SYSCALL, npid, NULL, NULL);
        printf("clone pid=%ld\n", npid);
        continue;
      } else if (WSTOPSIG(status) == (SIGTRAP | 0x80)) {
        printf("syscall\n");
      } else if (WSTOPSIG(status) == (SIGTRAP)) {
        printf("trap\n");
      } else {
        printf("stopped by signal %d\n", WSTOPSIG(status));
      }
    }

    ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
  }

  return 0;
}
