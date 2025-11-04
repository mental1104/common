use mental1104::prelude::*;

fn main() {
    let a = [1, 2, 3, 5, 8];
    println!("has 5? {}", contains(SortedSlice(&a), &5));
}
