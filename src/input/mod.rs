#![allow(unused)]

use input::event::KeyboardEvent;
use input::event::keyboard::KeyboardEventTrait;
use input::{Event, Libinput, LibinputInterface};
use libc::{O_RDONLY, O_RDWR, O_WRONLY};
use std::fs::{File, OpenOptions};
use std::io::Result as IoResult;
use std::os::fd::FromRawFd;
use std::os::unix::io::{AsRawFd, RawFd};
use std::os::unix::{fs::OpenOptionsExt, io::OwnedFd};
use std::path::Path;

pub struct Interface;

impl LibinputInterface for Interface {
    fn open_restricted(&mut self, path: &Path, flags: i32) -> Result<OwnedFd, i32> {
        open_via_logind(path, flags)
    }

    fn close_restricted(&mut self, fd: OwnedFd) {
        drop(File::from(fd));
    }
}

fn open_via_logind(path: &Path, flags: i32) -> Result<OwnedFd, i32> {
    // Check if we're in an active session by looking for session environment
    let session_vars = ["XDG_SESSION_ID", "XDG_SESSION_TYPE", "XDG_RUNTIME_DIR"];
    let has_session = session_vars.iter().any(|var| std::env::var(var).is_ok());

    if !has_session {
        return Err(libc::EACCES);
    }

    // Try direct access (works if user has proper session permissions)
    match OpenOptions::new()
        .custom_flags(flags)
        .read(true)
        .write(true)
        .open(path)
    {
        Ok(file) => {
            // Convert File to OwnedFd
            use std::os::unix::io::IntoRawFd;
            let raw_fd = file.into_raw_fd();
            unsafe { Ok(OwnedFd::from_raw_fd(raw_fd)) }
        }
        Err(e) => Err(e.raw_os_error().unwrap_or(libc::EIO)),
    }
}
