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

  pid_t top_process = atoi(argv[1]);
  printf("attach to %d\n", top_process);

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
    if (WIFEXITED(status) && pid == top_process) {
      printf("exited, status=%d pid=%d\n", WEXITSTATUS(status) , pid);
      break;
    } else if (WIFSIGNALED(status)) {
      printf("terminated by signal %d\n", WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
      int event = (status >> 8);
      long npid;
      if(event == (SIGTRAP | PTRACE_EVENT_VFORK << 8)) {
        ptrace(PTRACE_GETEVENTMSG, pid, NULL, &npid);
        printf("vfor pid=%ld\n", npid);
      } else if (event == (SIGTRAP | PTRACE_EVENT_FORK << 8)) {
        ptrace(PTRACE_GETEVENTMSG, pid, NULL, &npid);
        printf("fork pid=%ld\n", npid);
      } else if (event == (SIGTRAP | PTRACE_EVENT_CLONE << 8)) {
        ptrace(PTRACE_GETEVENTMSG, pid, NULL, &npid);
        printf("clone pid=%ld\n", npid);
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
