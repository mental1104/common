use experimental_hello::{extract_world, HELLO};

#[test]
fn integration_world_is_found() {
    assert_eq!(extract_world(HELLO), Some("world"));
}
