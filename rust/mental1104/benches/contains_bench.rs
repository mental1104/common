use criterion::{black_box, criterion_group, criterion_main, Criterion};
use mental1104::prelude::*;
use std::collections::HashSet;

fn bench(c: &mut Criterion) {
    let v: Vec<_> = (0..4096).collect();
    let sorted = SortedSlice(&v);
    let hs: HashSet<_> = v.iter().copied().collect();

    c.bench_function("slice_binary_4k", |b| {
        b.iter(|| black_box(contains(sorted, &2047)))
    });

    c.bench_function("hashset_4k", |b| {
        b.iter(|| black_box(contains(&hs, &2047)))
    });
}

criterion_group!(benches, bench);
criterion_main!(benches);
