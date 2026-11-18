//! 检查文件能力经公开 prelude 可被独立集成测试使用。
use mental1104::prelude::{move_file, MoveStatus};

/// 最小完整用法：公开导入、准备源、调用、读取结果并回收资源；Windows 持久化明确受限。
#[test]
fn minimal_complete_usage() -> std::io::Result<()> {
    let path = std::env::temp_dir().join(format!("common-move-it-{}", std::process::id()));
    std::fs::create_dir(&path)?;
    let source = path.join("source");
    let target = path.join("target");
    // 写入后关闭源，调用公开入口。
    std::fs::write(&source, b"hello")?;
    let result = move_file(&source, &target, false)?;
    // 核验状态和磁盘结果，然后显式释放测试目录。
    assert_eq!(
        result.status,
        if cfg!(windows) {
            MoveStatus::DurabilityUncertain
        } else {
            MoveStatus::Completed
        }
    );
    assert!(result.source_removed);
    assert_eq!(std::fs::read(&target)?, b"hello");
    assert!(!source.exists());
    std::fs::remove_dir_all(path)?;
    Ok(())
}
