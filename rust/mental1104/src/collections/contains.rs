use std::borrow::Borrow;
use std::collections::{BTreeMap, BTreeSet, HashMap, HashSet};
use std::hash::Hash;

/// 统一接口：判断容器是否“包含某个 key”
///
/// Rust 不支持按参数类型重载；这里用 **Trait + 泛型** 实现“同名静态多态”。
/// 编译期根据 `C` 的具体类型选择对应 `impl`，无虚表开销（单态化 + 内联）。
/// 关键点：`Q: ?Sized` 以支持 `&str` 这类 DST（动态大小类型）。
pub trait HasKey<Q: ?Sized> {
    /// 是否包含 `key`
    fn contains(&self, key: &Q) -> bool;
}

/// 语义新类型：**已排序切片**（升序）
///
/// 切片本身不携带“是否有序”的元信息。若要在编译期选择二分搜索（`O(log n)`），
/// 需把“已排序”提升为类型信息，由调用方显式承诺。
#[repr(transparent)]
#[derive(Copy, Clone)] // ✅ 允许在基准闭包中按值多次捕获（修复 E0507）
pub struct SortedSlice<'a, T>(pub &'a [T]);

/// 对外统一入口：名称就叫 `contains`。
///
/// - 看起来像“函数重载”，实则依赖 `C: HasKey<Q>` 的静态分发；
/// - 线性切片 vs 二分切片 vs Hash/BTree 容器，由各自 `impl` 决定；
/// - **技术原理**（零成本抽象）：
///   1) **静态多态**（Traits + Generics）：编译器按 `C` 的具体类型选定 `impl`;  
///   2) **单态化**（Monomorphization）：为每种 `C/Q` 组合生成专门版本；  
///   3) **内联**：优化后与直接调用底层容器 API 等价。
#[inline]
pub fn contains<C, Q: ?Sized>(container: C, key: &Q) -> bool
where
    C: HasKey<Q>,
{
    container.contains(key)
}

/* ---------------- 切片 / Vec ---------------- */

/// 无序切片：线性扫描 O(n)；常数小，n 小时很快。
impl<'a, T, Q> HasKey<Q> for &'a [T]
where
    Q: ?Sized,
    T: PartialEq<Q>,
{
    #[inline]
    fn contains(&self, key: &Q) -> bool {
        // 避免与本 trait 的 contains 同名而递归：使用迭代器比较
        self.iter().any(|t| t == key)
    }
}

/// 已排序切片：二分查找 O(log n)；n 大或多次查优势显著。
impl<'a, T, Q> HasKey<Q> for SortedSlice<'a, T>
where
    T: Borrow<Q>,
    Q: Ord + ?Sized,
{
    #[inline]
    fn contains(&self, key: &Q) -> bool {
        self.0
            .binary_search_by(|probe| probe.borrow().cmp(key))
            .is_ok()
    }
}

/* ---------------- Map / Set ---------------- */

/// HashMap：平均 O(1)，支持异型查询（K=String, Q=str）零拷贝桥接。
impl<'a, K, V, Q> HasKey<Q> for &'a HashMap<K, V>
where
    K: Borrow<Q> + Eq + Hash,
    Q: Eq + Hash + ?Sized,
{
    #[inline]
    fn contains(&self, key: &Q) -> bool {
        self.contains_key(key)
    }
}

/// BTreeMap：O(log n)，同样支持 Borrow 桥接。
impl<'a, K, V, Q> HasKey<Q> for &'a BTreeMap<K, V>
where
    K: Borrow<Q> + Ord,
    Q: Ord + ?Sized,
{
    #[inline]
    fn contains(&self, key: &Q) -> bool {
        self.contains_key(key)
    }
}

/// HashSet：平均 O(1)。
impl<'a, T, Q> HasKey<Q> for &'a HashSet<T>
where
    T: Borrow<Q> + Eq + Hash,
    Q: Eq + Hash + ?Sized,
{
    #[inline]
    fn contains(&self, key: &Q) -> bool {
        // 显式限定到标准库的 HashSet::contains，避免与本 trait 名称冲突。
        HashSet::contains(self, key)
    }
}

/// BTreeSet：O(log n)。
impl<'a, T, Q> HasKey<Q> for &'a BTreeSet<T>
where
    T: Borrow<Q> + Ord,
    Q: Ord + ?Sized,
{
    #[inline]
    fn contains(&self, key: &Q) -> bool {
        // 显式限定到标准库的 BTreeSet::contains，避免与本 trait 名称冲突。
        BTreeSet::contains(self, key)
    }
}

/* ---------------- 单元测试（模块内） ---------------- */

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn slice_unsorted() {
        let v = vec![3, 1, 4];
        assert!(contains(v.as_slice(), &1));
        assert!(!contains(v.as_slice(), &2));
    }

    #[test]
    fn slice_sorted() {
        let a = [1, 2, 3, 5, 8];
        assert!(contains(SortedSlice(&a), &5));
        assert!(!contains(SortedSlice(&a), &4));
    }

    #[test]
    fn maps_sets() {
        let mut hm: HashMap<String, i32> = HashMap::new();
        hm.insert("alice".into(), 1);
        assert!(contains(&hm, "alice"));
        assert!(!contains(&hm, "bob"));

        let mut bm: BTreeMap<i32, &str> = BTreeMap::new();
        bm.insert(42, "answer");
        assert!(contains(&bm, &42));

        let hs: HashSet<&str> = ["a", "b"].into_iter().collect();
        assert!(contains(&hs, "a"));

        let bs: BTreeSet<i32> = [1, 2, 3].into_iter().collect();
        assert!(contains(&bs, &2));
    }
}
