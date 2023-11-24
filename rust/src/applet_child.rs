use std::{ffi::OsString, time::Duration, thread::sleep};

use clap::Parser;
use nix::{libc::getpid, sched::{unshare, CloneFlags}, unistd::{getuid, getgid}};

#[derive(Parser, Debug)]
#[command(name = "child")]
struct Args {

    /// Command to run in the spawned child
    #[arg(long, short, default_value_t)]
    command: String,
}


const WAIT_INTERVAL: Duration = Duration::from_millis(10);

fn wait_root() {
    for _ in 0..1000 {
        if getuid().is_root() && getgid().as_raw() == 0 {
            return
        }
        sleep(WAIT_INTERVAL)
    }
    panic!("We're not mapped to root")
}

pub(crate) fn main<I, S>(args: I) 
where
    I: Iterator<Item = S>,
    S: Into<OsString> + Clone,
{
    unshare(CloneFlags::CLONE_NEWUSER | CloneFlags::CLONE_NEWNS | CloneFlags::CLONE_NEWPID).unwrap();
    wait_root();
    let args = Args::parse_from(args);
    let mut command = std::process::Command::new("/bin/bash");
    if ! args.command.is_empty() {
        command.arg("-c")
            .arg(args.command);
    };
    command
        .spawn()
        .unwrap()
        .wait()
        .unwrap();
}