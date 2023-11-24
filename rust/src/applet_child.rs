use std::ffi::OsString;

use clap::Parser;
use nix::{libc::getpid, sched::{unshare, CloneFlags}};

#[derive(Parser, Debug)]
#[command(name = "child")]
struct Args {

    /// Command to run in the spawned child
    #[arg(long, short)]
    command: String,
}

pub(crate) fn main<I, S>(args: I) 
where
    I: Iterator<Item = S>,
    S: Into<OsString> + Clone,
{
    let pid = unsafe {getpid()};
    println!("Child pid is {}", pid);
    unshare(CloneFlags::CLONE_NEWUSER | CloneFlags::CLONE_NEWNS | CloneFlags::CLONE_NEWPID).unwrap();
    std::thread::sleep(std::time::Duration::from_secs(1));
    let args = Args::parse_from(args);
    std::process::Command::new("/bin/bash")
        .arg("-c")
        .arg(args.command)
        .spawn()
        .unwrap()
        .wait()
        .unwrap();
}