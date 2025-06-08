use ::input::Libinput;

mod input;

fn main() {
    println!("start...");
    let mut input = Libinput::new_with_udev(crate::input::Interface);
    loop {
        input.dispatch().unwrap();
        for event in &mut input {
            println!("Event:{:?}", event);

        }
    }
}
