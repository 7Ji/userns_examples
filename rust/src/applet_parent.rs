use std::{fs::read_link, ffi::OsString};

use clap::Parser;

#[derive(Parser, Debug)]
#[command(name = "parent")]
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
    let args = Args::parse_from(args);
    let idmaps = crate::idmap::IdMaps::new();
    println!("Idmaps: {:?}", idmaps);
    let exe = read_link("/proc/self/exe").unwrap();
    let mut child = std::process::Command::new(exe)
        .arg("child")
        .arg("--command")
        .arg(args.command)
        .spawn()
        .unwrap();
    println!("Spawned child {}", child.id());
    std::thread::sleep(std::time::Duration::from_millis(100));
    idmaps.set_pid(child.id() as nix::libc::pid_t);

    child.wait().unwrap();
}
