//! rust-todo - P4 SDK sample console app (specs/2026-09-16-p4-sdk-contract SS6).
//!
//! Same contract as sampletodo.cmd, in Rust:
//!   no args -> print todo.txt (UTF-8, one item per line)
//!   any arg -> hand the list to the agent via jkctl ask

use std::fs;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    let here = std::env::current_exe()
        .ok()
        .and_then(|p| p.parent().map(|d| d.to_path_buf()))
        .unwrap_or_else(|| PathBuf::from("."));
    // exe lives in target/release/, app files live at the app root.
    let here = here
        .ancestors()
        .nth(2)
        .map(|p| p.to_path_buf())
        .unwrap_or(here);
    let jkctl = here.join(r"..\..\jkctl.exe");
    let todo = here.join("todo.txt");

    let items = fs::read_to_string(&todo)
        .map(|s| {
            s.lines()
                .filter(|l| !l.trim().is_empty())
                .map(String::from)
                .collect::<Vec<_>>()
        })
        .unwrap_or_default();

    let args: Vec<String> = std::env::args().skip(1).collect();
    if !args.is_empty() {
        let listing = items.join("; ");
        let status = Command::new(jkctl)
            .arg("ask")
            .arg(format!(
                "Review this todo list and set priorities: {listing}"
            ))
            .status();
        if let Err(e) = status {
            eprintln!("jkctl.exe not found next to apps/ ({e})");
            std::process::exit(1);
        }
        return;
    }

    if items.is_empty() {
        println!("(empty - add a line to todo.txt)");
        return;
    }
    for (i, item) in items.iter().enumerate() {
        println!("{}. {}", i + 1, item);
    }
}