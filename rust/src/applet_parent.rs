use std::{fs::read_link, ffi::{OsString, OsStr}, path::Path, time::Duration, thread::sleep};

use clap::Parser;

#[derive(Parser, Debug)]
#[command(name = "parent")]
struct Args {

    /// Command to run in the spawned child
    #[arg(long, short, default_value_t)]
    command: String,
}


const WAIT_INTERVAL: Duration = Duration::from_millis(10);

fn wait_unshare<P: AsRef<Path>, Q: AsRef<Path>>(link: P, ns_user: Q) {
    for _ in 0..1000 {
        let current_ns_user = read_link(&link).unwrap();
        if current_ns_user.as_path() == ns_user.as_ref() {
            return
        }
        sleep(WAIT_INTERVAL)
    }
    panic!("Child did not unshare")

}

pub(crate) fn main<I, S>(args: I) 
where
    I: Iterator<Item = S>,
    S: Into<OsString> + Clone,
{
    let args = Args::parse_from(args);
    let idmaps = crate::idmap::IdMaps::new();
    let exe = read_link("/proc/self/exe").unwrap();
    let ns_user = read_link("/proc/self/ns/user").unwrap();
    let mut command = std::process::Command::new(exe);
    command.arg("child");
    if ! args.command.is_empty() {
        command.arg("--command")
            .arg(args.command);
    }
    let mut child = command
        .spawn()
        .unwrap();
    println!("Spawned child {}", child.id());
    wait_unshare(format!("/proc/{}/ns/user", child.id()), ns_user);
    idmaps.set_pid(child.id() as nix::libc::pid_t);
    child.wait().unwrap();
}
