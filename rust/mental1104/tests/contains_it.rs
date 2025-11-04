use mental1104::prelude::*;
use std::collections::HashSet;

#[test]
fn it_works_across_modules() {
    let hs: HashSet<&str> = ["x", "y"].into_iter().collect();
    assert!(contains(&hs, "x"));
}
