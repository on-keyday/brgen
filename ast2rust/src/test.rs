use std::{env, path};

pub fn execAndOutput(file: &str) -> std::io::Result<std::process::Output> {
    env::set_current_dir(path::Path::new("..")).unwrap();
    println!("current dir: {:?}", env::current_dir().unwrap());
    let cmd = env::current_dir()
        .unwrap()
        .join(path::Path::new("tool/src2json"));
    #[cfg(target_os = "windows")]
    let cmd = cmd.with_extension("exe");
    println!("cmd: {:?}", cmd);
    let mut cmd = std::process::Command::new(cmd);
    cmd.arg(file).output()
}
