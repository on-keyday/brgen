mod target;
use serde::Deserialize;
use serde_binary;
fn main() {
    let args: Vec<String> = std::env::args().collect();
    let input = std::fs::read(&args[1]).unwrap();
    let mut deserializer = serde_binary::Deserializer::from_slice(&input);
    let de = target::TEST_CLASS::deserialize(&mut deserializer);
}
