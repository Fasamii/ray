#![allow(unused_imports)]
use std::{
    io::Write,
    os::{fd::AsFd, unix::ffi::OsStrExt},
};

use input::{Event, Libinput, LibinputInterface};
use libseat::{Seat, SeatEvent, SeatRef};

struct Interface {
    seat: Seat,
}

impl Interface {
    fn new() -> Self {
        let seat = Seat::open(|seat, event| {
            println!("inside seat:{seat:?}");
            println!("inside event:{event:?}");
        })
        .unwrap();
        Interface { seat }
    }
}

impl LibinputInterface for Interface {
    fn open_restricted(
        &mut self,
        path: &std::path::Path,
        _: i32,
    ) -> Result<std::os::unix::prelude::OwnedFd, i32> {
        Ok(self
            .seat
            .open_device(&path)
            .expect("failed to oepn device")
            .as_fd()
            .try_clone_to_owned()
            .expect("failed to own fd"))
    }

    fn close_restricted(&mut self, fd: std::os::unix::prelude::OwnedFd) {
        let _ = fd;
    }
}

fn main() {
    let mut input = Libinput::new_with_udev(Interface::new());
    input.udev_assign_seat("seat0").expect("whoops");
    println!("opened input");
    loop {
        input.dispatch().unwrap();
        print!("*");
        for event in &mut input {
            println!("Got event: {:?}", event);
            std::io::stdout().flush().expect("failed to flush");
        }
    }
}
