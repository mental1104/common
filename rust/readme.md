## 安装rust

你的表达没问题。

# 操作（只给最优解）

> 目标：`cargo test` 需要编译 dev-deps（含 Criterion）。你当前 rustc=1.75，Criterion 间接依赖的 `rayon-core=1.13` 需 **>=1.80**。**最稳做法：用 rustup 升到稳定版（>=1.80）并让本仓库强制用它。**

**一步步执行：**

```bash
# 1) 安装/初始化 rustup（如已装可跳过）
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
source "$HOME/.cargo/env"            # 刷新 PATH；之后 `cargo -V` 应来自 ~/.cargo/bin

# 2) 安装/切到稳定版（会是 1.8x）
rustup toolchain install stable
rustup default stable                 # 全局用 stable
# 或者仅对本项目锁定：
cd ~/code/common/rust/mental1104
rustup override set stable

# 3) 确认版本
rustc -V
cargo -V

# 4) 重新测试
cd ~/code/common
./dev test rust
```

> 如果你“现在**只想先跑通测试**，不跑基准”，临时方案（不升级编译器）：
>
> * 打开 `common/rust/mental1104/Cargo.toml`，**注释掉**这两段：
>
>   ```toml
>   [dev-dependencies]
>   # criterion = "0.5"
>
>   [[bench]]
>   # name = "contains_bench"
>   # harness = false
>   ```
> * 然后执行：
>
>   ```bash
>   cargo clean
>   ./dev test rust
>   ```
>
> 之后再升级到 rust 1.80+，把上面两段恢复即可。

---

# 原理（为什么这样做）

* **`cargo test` 会编译 dev-dependencies**，你的基准依赖 `criterion 0.5`，其依赖链拉到 `rayon-core 1.13`，要求 **rustc ≥ 1.80**；而你当前 **1.75**（多半是 apt 安装的旧版工具链）。
* **用 rustup 管理工具链** 能在 Ubuntu/WSL 下快速切到最新稳定版；配合项目内的 `rust-toolchain.toml`/`rustup override`，确保 **本项目必用新编译器**，不受系统包管理器影响。
* 临时注释 `criterion` 与 `[[bench]]` 可规避 dev-deps 编译，从而让测试先通过；但这只是权宜之计，**升级到 1.80+ 才是长期解**。


rustup component add llvm-tools-preview
cargo install cargo-llvm-cov
