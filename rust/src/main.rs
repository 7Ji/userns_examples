use std::{env::ArgsOs, path::PathBuf, os::unix::ffi::OsStrExt, ffi::OsString};


mod applet_child;
mod applet_parent;
mod idmap;

fn private_args(args: ArgsOs) -> impl Iterator<Item = OsString> {
    std::iter::once(OsString::new()).chain(args.into_iter())
}

fn dispatch(mut args: ArgsOs) {
    match PathBuf::from(args.nth(0).unwrap()).file_name().unwrap().as_bytes() {
        b"userns-example" => dispatch(args),
        b"parent" => applet_parent::main(private_args(args)),
        b"child" => applet_child::main(private_args(args)),
        other => println!("Unknown applet {}", String::from_utf8_lossy(other)),
    }
}

fn main() {
    dispatch(std::env::args_os())
}
