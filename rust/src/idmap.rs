use std::{fs::File, path::Path, io::{BufRead, BufReader, Read}, ffi::{OsStr, OsString}, os::unix::ffi::OsStrExt, fmt::format, process::Command};

use nix::{unistd::{Uid, getuid, getgid}, libc::{uid_t, gid_t, pid_t}};

#[derive(Debug)]
struct IdMap {
    out_me: uid_t,
    out_sub_start: uid_t,
    // out_sub_range: uid_t,
}

#[derive(Debug)]
pub(crate) struct IdMaps {
    uid_map: IdMap,
    gid_map: IdMap
}

#[derive(Debug)]
struct SubId {
    start: uid_t,
    range: uid_t
}

impl SubId {
    fn from_file<P: AsRef<Path>, S: AsRef<str>>(path: P, id: uid_t, name: S) -> Self {
        let mut file = File::open(path).unwrap();
        let mut buffer = Vec::new();
        file.read_to_end(&mut buffer).unwrap();
        let id = format!("{}", id);
        for line in buffer.split(|c|*c == b'\n')  {
            if line.is_empty() { continue }
            let mut segments = line.split(|c|*c == b':');
            let identifier = segments.next().unwrap();
            if identifier != id.as_bytes() && identifier != name.as_ref().as_bytes() { continue }
            let start = segments.next().unwrap();
            let range = segments.next().unwrap();
            if segments.next().is_some() { panic!("subid file contains illegal line") }
            let range: uid_t = String::from_utf8_lossy(range).parse().unwrap();
            if range >= 65535 {
                return Self { start: String::from_utf8_lossy(start).parse().unwrap() , range }
            }
        }
        panic!("Cannot find subid config")
    }
}

impl IdMap {
    fn set_pid(&self, pid: pid_t, prog: &str) {
        let mut command = Command::new(prog);
        command
            .arg(format!("{}", pid))
            .arg("0")
            .arg(format!("{}", self.out_me))
            .arg("1")
            .arg("1")
            .arg(format!("{}", self.out_sub_start))
            .arg("65535");
        println!("Command is {:?}", command);
        if ! command
            .spawn()
            .unwrap()
            .wait_with_output()
            .unwrap()
            .status
            .success() 
        {
            panic!("Failed to set new idmap")
        }
    }
}


impl IdMaps {
    pub(crate) fn new() -> Self {
        let uid = getuid();
        let passwd = passwd::Passwd::from_uid(uid.as_raw()).unwrap();
        let name = passwd.name;
        let subuid = SubId::from_file("/etc/subuid", uid.as_raw(), &name);
        let gid = getgid();
        let subgid = SubId::from_file("/etc/subgid", gid.as_raw(), &name);
        return Self {
            uid_map: IdMap { out_me: uid.as_raw(), out_sub_start: subuid.start },
            gid_map: IdMap { out_me: gid.as_raw(), out_sub_start: subgid.start },
        }
    }

    pub(crate) fn set_pid(&self, pid: pid_t) {
        self.uid_map.set_pid(pid, "/usr/bin/newuidmap");
        self.gid_map.set_pid(pid, "/usr/bin/newgidmap");
    }
}