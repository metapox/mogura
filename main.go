package main

import (
	"flag"
	"syscall"
)

func main() {
	// ptrace
	// watch process time
	pid := flag.Int("p", -1, "pid")
	flag.Parse()

	if *pid == -1 {
		panic("pid is required")
	}
	err := syscall.PtraceAttach(*pid)
	if err != nil {
		panic(err)
	}

}
