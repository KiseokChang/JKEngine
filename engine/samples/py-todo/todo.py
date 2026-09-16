#!/usr/bin/env python3
"""py-todo - P4 SDK sample console app (specs/2026-09-16-p4-sdk-contract SS6).

Mirror of the sampletodo.cmd contract, in Python:
  no args  -> print todo.txt (UTF-8, one item per line)
  any arg  -> hand the list to the agent via jkctl ask for prioritization
"""

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
JKCTL = os.path.normpath(os.path.join(HERE, "..", "..", "jkctl.exe"))


def main(argv):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except AttributeError:
        pass  # Python < 3.7

    todo_path = os.path.join(HERE, "todo.txt")
    try:
        with open(todo_path, encoding="utf-8") as f:
            items = [line.rstrip("\n") for line in f if line.strip()]
    except OSError:
        print("(todo.txt not found - write one item per line)")
        return 0

    if not argv:
        for i, item in enumerate(items, 1):
            print(f"{i}. {item}")
        if not items:
            print("(empty - add a line to todo.txt)")
        return 0

    # Agent path: jkctl ask prints the streaming LLM reply in this terminal.
    listing = "; ".join(items)
    try:
        return subprocess.call([JKCTL, "ask",
                                "Review this todo list and set priorities: "
                                + listing])
    except FileNotFoundError:
        print("jkctl.exe not found next to apps/ - install the app under "
              "the desktop build root")
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))