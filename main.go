package main

import (
	"fmt"
	"os"
	"os/exec"

	"syscall"
	"time"

	"golang.org/x/sys/unix"
)

func main() {
	// pid := flag.Int("p", -1, "pid")
	// flag.Parse()

	cmd := exec.Command("./test/test.sh")
	err := cmd.Start()

	if err != nil {
		panic(err)
	}
	pid := &cmd.Process.Pid

	if *pid == -1 {
		panic("pid is required")
	}
	err = syscall.PtraceAttach(*pid)
	if err != nil {
		panic(err)
	}

	sig := unix.Siginfo{}
	unix.Waitid(unix.P_PID, *pid, &sig, 0, nil)
	err = syscall.PtraceSetOptions(*pid, syscall.PTRACE_O_TRACESYSGOOD|syscall.PTRACE_O_TRACEEXEC|syscall.PTRACE_O_TRACECLONE|syscall.PTRACE_O_TRACEFORK|syscall.PTRACE_O_TRACEVFORK|syscall.PTRACE_O_TRACEEXEC)
	if err != nil {
		panic(err)
	}
	syscall.PtraceCont(*pid, 0)
	watchProcess(*pid)
}

func watchProcess(pid int) {
	fmt.Println("Watching process", pid)
	startTime := time.Now()

	for {
		var sig unix.Siginfo
		npid, err := unix.Waitid(unix.P_PID, pid, &sig, 0, nil)

		if err != nil {
			fmt.Println("Error waiting for the process to exit:", err)
			os.Exit(1)
		}

		if ws.Exited() {
			break
		} else if ws.Stopped() && ws.StopSignal() == syscall.SIGTRAP {
			if ws.TrapCause() != 0 {
				npid, err := syscall.PtraceGetEventMsg(pid)
				if err != nil {
					fmt.Println("Error getting event message:", err)
					os.Exit(1)
				}
				fmt.Println("New process created:", npid)
			}
		}

		// } else if ws.StopSignal() == syscall.SIGTRAP|0x80 {
		// 	fmt.Println("Process TrapCause:", ws.TrapCause())
		// 	time.Sleep(5 * time.Second)
		// }

		// fmt.Println("Process stopped. Signal:", ws.StopSignal())

		syscall.PtraceCont(pid, 0)
	}

	endTime := time.Now()
	executionTime := endTime.Sub(startTime)

	fmt.Printf("pid %v exited. Execution time: %v\n", pid, executionTime)
}
