package main

import (
	"fmt"
	"os"

	"flag"
	"syscall"
	"time"
)

func main() {
	pid := flag.Int("p", -1, "pid")
	flag.Parse()

	// cmd := exec.Command("./test/test.sh")
	// err := cmd.Start()

	// if err != nil {
	// 	panic(err)
	// }

	if *pid == -1 {
		panic("pid is required")
	}
	err := syscall.PtraceAttach(*pid)
	if err != nil {
		panic(err)
	}

	var ws syscall.WaitStatus
	_, err = syscall.Wait4(*pid, &ws, 0, nil)
	if err != nil {
		panic(err)
	}

	startTime := time.Now()
	err = syscall.PtraceCont(*pid, 0)

	if _, err = syscall.Wait4(*pid, &ws, 0, nil); err != nil {
		fmt.Println("Error waiting for the process to exit:", err)
		os.Exit(1)
	}

	// Record end time and calculate execution time
	endTime := time.Now()
	executionTime := endTime.Sub(startTime)

	fmt.Printf("Child process exited. Execution time: %v\n", executionTime)
}
