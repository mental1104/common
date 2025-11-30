pub const HELLO: &str = "hello world";

pub fn extract_world(input: &str) -> Option<&str> {
    input.split_whitespace().find(|part| *part == "world")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn finds_world() {
        assert_eq!(extract_world(HELLO), Some("world"));
    }

    #[test]
    fn returns_none_when_missing() {
        assert_eq!(extract_world("no match"), None);
    }
}
