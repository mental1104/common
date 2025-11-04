//! mental1104 - Rust 公共库
//!
//! - 设计与 `python/mental1104`、`golang/mental1104` 呼应：提供常用工具与统一接口。
//! - 暴露 `prelude` 便于外部一次性引入常用符号。
//! - collections::contains：统一“是否包含 key”的静态多态 API（Trait + 泛型）。

pub mod prelude;
pub mod macros;
pub mod error;
pub mod collections;

// 可选：暴露常用类型别名（例如更快的 HashMap/HashSet）
#[cfg(feature = "fast-hash")]
pub use ahash::{AHashMap as FastHashMap, AHashSet as FastHashSet};
