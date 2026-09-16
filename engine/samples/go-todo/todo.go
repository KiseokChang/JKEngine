// go-todo - P4 SDK sample console app (specs/2026-09-16-p4-sdk-contract SS6).
//
// Same contract as sampletodo.cmd, in Go:
//   no args -> print todo.txt (UTF-8, one item per line)
//   any arg -> hand the list to the agent via jkctl ask
package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
)

func main() {
	exe, err := os.Executable()
	if err != nil {
		fmt.Fprintln(os.Stderr, "cannot locate exe:", err)
		os.Exit(1)
	}
	here := filepath.Dir(exe)
	jkctl := filepath.Join(here, "..", "..", "jkctl.exe")

	raw, err := os.ReadFile(filepath.Join(here, "todo.txt"))
	items := []string{}
	if err == nil {
		for _, line := range strings.Split(string(raw), "\n") {
			line = strings.TrimRight(line, "\r")
			if strings.TrimSpace(line) != "" {
				items = append(items, line)
			}
		}
	}

	if len(os.Args) > 1 {
		listing := strings.Join(items, "; ")
		cmd := exec.Command(jkctl, "ask",
			"Review this todo list and set priorities: "+listing)
		cmd.Stdout, cmd.Stderr = os.Stdout, os.Stderr
		if err := cmd.Run(); err != nil {
			fmt.Fprintln(os.Stderr, "jkctl.exe not found next to apps/:", err)
			os.Exit(1)
		}
		return
	}

	if len(items) == 0 {
		fmt.Println("(empty - add a line to todo.txt)")
		return
	}
	for i, item := range items {
		fmt.Printf("%d. %s\n", i+1, item)
	}
}