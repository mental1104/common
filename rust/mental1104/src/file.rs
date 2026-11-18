//! 文件、目录与符号链接的移动；跨卷发布和源清理具有独立的失败状态。

use std::fs::{self, File, FileTimes, Metadata, OpenOptions};
use std::io::{self, Read, Seek, SeekFrom, Write};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};

/// 已发布的最终状态；持久化不确定时另查 source_removed。
#[derive(Debug, PartialEq, Eq)]
pub enum MoveStatus {
    /// 源已清理且必要的目录同步成功。
    Completed,
    /// 目标已发布，但源可能只完成部分清理。
    PublishedSourceRemains,
    /// 命名成功，但目录持久化无法确认。
    DurabilityUncertain,
}

/// 已发布结果；error 为空仅表示完成，发布前错误由外层 io::Result 表达。
#[derive(Debug)]
pub struct MoveResult {
    /// 最终阶段，持久化失败优先报告。
    pub status: MoveStatus,
    /// 源根目录项是否已删除。
    pub source_removed: bool,
    /// 部分成功时的错误原因。
    pub error: Option<io::Error>,
}

/// 一个源路径及其不跟随链接的快照，不承担文件资源所有权。
struct Entry {
    path: PathBuf,
    info: Metadata,
}

/// 本次移动的私有临时容器，析构仅尽力回收本次产物。
struct Stage(PathBuf);

impl Stage {
    /// 在 parent 中独占创建 0700 容器；返回拥有清理责任的 guard。
    fn create(parent: &Path) -> io::Result<Self> {
        static SEQUENCE: AtomicU64 = AtomicU64::new(0);
        for _ in 0..128 {
            let sequence = SEQUENCE.fetch_add(1, Ordering::Relaxed);
            let name = format!(".move-{}-{}", std::process::id(), sequence);
            let path = parent.join(name);
            #[cfg(unix)]
            let mut builder = fs::DirBuilder::new();
            #[cfg(not(unix))]
            let builder = fs::DirBuilder::new();
            #[cfg(unix)]
            {
                use std::os::unix::fs::DirBuilderExt;
                builder.mode(0o700);
            }
            match builder.create(&path) {
                Ok(()) => return Ok(Self(path)),
                Err(e) if e.kind() == io::ErrorKind::AlreadyExists => continue,
                Err(e) => return Err(e),
            }
        }
        Err(io::Error::new(
            io::ErrorKind::AlreadyExists,
            "cannot allocate private staging directory",
        ))
    }
}

impl Drop for Stage {
    /// 不传播清理错误，避免覆盖发布前原始故障；绝不回滚已经发布的目标。
    fn drop(&mut self) {
        discard(&self.0);
    }
}

/// 尽力清理私有 path；目录可能已恢复为只读，先恢复清理权限，不跟随链接，不影响源或已发布目标。
fn discard(path: &Path) {
    let Ok(info) = fs::symlink_metadata(path) else {
        return;
    };
    if info.is_dir() {
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            if fs::set_permissions(path, fs::Permissions::from_mode(0o700)).is_err() {
                return;
            }
        }
        if let Ok(items) = fs::read_dir(path) {
            for item in items.flatten() {
                discard(&item.path());
            }
        }
        let _ = fs::remove_dir(path);
    } else {
        let _ = fs::remove_file(path);
    }
}

/// 返回 path 的非空父目录；裸文件名采用当前目录。
fn parent(path: &Path) -> &Path {
    path.parent()
        .filter(|p| !p.as_os_str().is_empty())
        .unwrap_or(Path::new("."))
}

/// 构造 ASCII 的冲突错误；message 说明当前失败阶段。
fn conflict(message: &'static str) -> io::Error {
    io::Error::other(message)
}

/// 为输入 path 保留目录项语义，拒绝根及尾部点段；返回借用原路径。
fn valid_path(path: &Path) -> io::Result<&Path> {
    if path.as_os_str().is_empty() || path.file_name().is_none() {
        return Err(conflict("invalid move path"));
    }
    Ok(path)
}

#[cfg(unix)]
/// 比较 a/b 的设备、inode、类型及权限，返回是否仍是同一目录项对象。
fn same_identity(a: &Metadata, b: &Metadata) -> bool {
    use std::os::unix::fs::MetadataExt;
    a.dev() == b.dev() && a.ino() == b.ino() && a.mode() == b.mode()
}

#[cfg(unix)]
/// 比较 a/b 的 ctime 纳秒值，检测修改后恢复 mtime 的情况。
fn same_change_time(a: &Metadata, b: &Metadata) -> bool {
    use std::os::unix::fs::MetadataExt;
    a.ctime() == b.ctime() && a.ctime_nsec() == b.ctime_nsec()
}

#[cfg(not(unix))]
/// 非 Unix 跨卷禁用；a/b 仅用于本地复制代码的编译，不提供身份承诺。
fn same_identity(a: &Metadata, b: &Metadata) -> bool {
    a.file_type() == b.file_type() && a.len() == b.len()
}

#[cfg(not(unix))]
/// 非 Unix 跨卷禁用，a/b 时间比较不作为安全保证。
fn same_change_time(a: &Metadata, b: &Metadata) -> bool {
    a.modified().ok() == b.modified().ok()
}

/// 验证 entry 仍未改变；identity_only 用于删除子项后时间已改变的目录。
fn unchanged(entry: &Entry, identity_only: bool) -> io::Result<()> {
    let now = fs::symlink_metadata(&entry.path)?;
    if !same_identity(&entry.info, &now)
        || (!identity_only
            && (entry.info.len() != now.len()
                || entry.info.modified()? != now.modified()?
                || !same_change_time(&entry.info, &now)))
    {
        return Err(conflict("source changed during move"));
    }
    Ok(())
}

/// 本次调用的 I/O 边界；默认均为真实操作，测试可逐项注入故障而不污染全局状态。
trait MoveIo {
    /// 原子移动 source 到 target；overwrite 决定是否允许替换，返回系统错误。
    fn rename(&self, source: &Path, target: &Path, overwrite: bool) -> io::Result<()> {
        native_rename(source, target, overwrite)
    }
    /// 同步 path 目录，无法确认持久化时返回错误。
    fn sync_dir(&self, path: &Path) -> io::Result<()> {
        File::open(path)?.sync_all()
    }
    /// 删除 entry 代表的一项，不递归删除未知项；失败返回系统错误。
    fn remove(&self, entry: &Entry) -> io::Result<()> {
        if entry.info.is_dir() {
            fs::remove_dir(&entry.path)
        } else {
            fs::remove_file(&entry.path)
        }
    }
    /// 将 input 复制到 output 并核对 expected 长度/读回数据，返回任何 I/O 错误。
    fn copy(&self, input: &mut File, output: &mut File, expected: u64) -> io::Result<()> {
        copy_bytes(input, output, expected)
    }
}

/// 无共享状态的系统 I/O 实现。
struct NativeIo;
impl MoveIo for NativeIo {}

/// 分块复制 input 到 output 并逐字节读回；expected 为原快照长度，文件由调用方关闭。
fn copy_bytes(input: &mut File, output: &mut File, expected: u64) -> io::Result<()> {
    let mut a = [0u8; 65536];
    let mut b = [0u8; 65536];
    let mut total = 0;
    loop {
        let n = input.read(&mut a)?;
        if n == 0 {
            break;
        }
        output.write_all(&a[..n])?;
        total += n as u64;
    }
    if total != expected {
        return Err(conflict("copy size verification failed"));
    }
    input.seek(SeekFrom::Start(0))?;
    output.seek(SeekFrom::Start(0))?;
    let mut remaining = expected;
    while remaining > 0 {
        let n = remaining.min(a.len() as u64) as usize;
        input.read_exact(&mut a[..n])?;
        output.read_exact(&mut b[..n])?;
        if a[..n] != b[..n] {
            return Err(conflict("copy readback verification failed"));
        }
        remaining -= n as u64;
    }
    if input.read(&mut a[..1])? != 0 || output.read(&mut b[..1])? != 0 {
        return Err(conflict("copy length changed"));
    }
    Ok(())
}

#[cfg(unix)]
/// 不跟随最后一级 path 链接打开源；返回由调用方拥有的 File。
fn open_source(path: &Path) -> io::Result<File> {
    use std::os::unix::fs::OpenOptionsExt;
    OpenOptions::new()
        .read(true)
        .custom_flags(libc::O_NOFOLLOW | libc::O_NONBLOCK)
        .open(path)
}

#[cfg(not(unix))]
/// 跨卷禁用平台的编译入口；path 的资源由返回 File 拥有。
fn open_source(path: &Path) -> io::Result<File> {
    File::open(path)
}

#[cfg(unix)]
/// 把 info 的修改时间恢复到 path 链接本身，不跟随链接；返回原始系统错误。
fn link_time(path: &Path, info: &Metadata) -> io::Result<()> {
    use std::os::unix::{ffi::OsStrExt, fs::MetadataExt};
    let name = std::ffi::CString::new(path.as_os_str().as_bytes())?;
    let time = libc::timespec {
        tv_sec: info.mtime() as _,
        tv_nsec: info.mtime_nsec() as _,
    };
    // name 和 times 在调用期间存活；AT_SYMLINK_NOFOLLOW 禁止修改链接目标。
    let rc = unsafe {
        libc::utimensat(
            libc::AT_FDCWD,
            name.as_ptr(),
            [time, time].as_ptr(),
            libc::AT_SYMLINK_NOFOLLOW,
        )
    };
    if rc == 0 {
        Ok(())
    } else {
        Err(io::Error::last_os_error())
    }
}

/// 复制 source 到私有 target，entries 追加源快照，ops 是当前调用的 I/O 边界。
/// 不跟随链接，特殊文件在发布前拒绝；普通权限和 mtime 恢复后再同步。
fn copy_tree(
    source: &Path,
    target: &Path,
    entries: &mut Vec<Entry>,
    ops: &impl MoveIo,
) -> io::Result<()> {
    let info = fs::symlink_metadata(source)?;
    if info.is_file() {
        let mut input = open_source(source)?;
        if !same_identity(&info, &input.metadata()?) {
            return Err(conflict("source replaced before copy"));
        }
        let mut options = OpenOptions::new();
        options.read(true).write(true).create_new(true);
        #[cfg(unix)]
        {
            use std::os::unix::fs::OpenOptionsExt;
            options.mode(0o600);
        }
        let mut output = options.open(target)?;
        ops.copy(&mut input, &mut output, info.len())?;
        #[cfg(unix)]
        {
            use std::os::unix::fs::{MetadataExt, PermissionsExt};
            output.set_permissions(fs::Permissions::from_mode(info.mode() & 0o777))?;
        }
        output.set_times(FileTimes::new().set_modified(info.modified()?))?;
        output.sync_all()?;
    } else if info.is_dir() {
        #[cfg(unix)]
        let mut builder = fs::DirBuilder::new();
        #[cfg(not(unix))]
        let builder = fs::DirBuilder::new();
        #[cfg(unix)]
        {
            use std::os::unix::fs::DirBuilderExt;
            builder.mode(0o700);
        }
        builder.create(target)?;
        // 先登记父项，使后续清理能按逆序删除子项。
        let index = entries.len();
        entries.push(Entry {
            path: source.to_owned(),
            info,
        });
        for child in fs::read_dir(source)? {
            let child = child?;
            copy_tree(&child.path(), &target.join(child.file_name()), entries, ops)?;
        }
        let original = &entries[index].info;
        let directory = File::open(target)?;
        #[cfg(unix)]
        {
            use std::os::unix::fs::{MetadataExt, PermissionsExt};
            directory.set_permissions(fs::Permissions::from_mode(original.mode() & 0o777))?;
        }
        directory.set_times(FileTimes::new().set_modified(original.modified()?))?;
        directory.sync_all()?;
        return unchanged(&entries[index], false);
    } else if info.is_symlink() {
        #[cfg(unix)]
        std::os::unix::fs::symlink(fs::read_link(source)?, target)?;
        #[cfg(not(unix))]
        return Err(conflict("cross-volume links unsupported"));
        #[cfg(unix)]
        link_time(target, &info)?;
    } else {
        return Err(conflict("unsupported source type"));
    }
    let entry = Entry {
        path: source.to_owned(),
        info,
    };
    unchanged(&entry, false)?;
    entries.push(entry);
    Ok(())
}

#[cfg(unix)]
/// 从 source 到 target 调用内核原子命名；overwrite=false 不允许存在性检查替代排他裁决。
fn native_rename(source: &Path, target: &Path, overwrite: bool) -> io::Result<()> {
    use std::os::unix::ffi::OsStrExt;
    if overwrite {
        return fs::rename(source, target);
    }
    let src = std::ffi::CString::new(source.as_os_str().as_bytes())?;
    let dst = std::ffi::CString::new(target.as_os_str().as_bytes())?;
    #[cfg(target_os = "linux")]
    // C 字符串在调用期间存活；内核只读路径，标记 1 为 RENAME_NOREPLACE。
    let rc = unsafe {
        libc::syscall(
            libc::SYS_renameat2,
            libc::AT_FDCWD,
            src.as_ptr(),
            libc::AT_FDCWD,
            dst.as_ptr(),
            1,
        )
    };
    #[cfg(target_os = "macos")]
    // RENAME_EXCL 由 Darwin 内核裁决，不存在 check-then-rename 窗口。
    let rc =
        unsafe { libc::renamex_np(src.as_ptr(), dst.as_ptr(), libc::RENAME_EXCL) } as libc::c_long;
    #[cfg(not(any(target_os = "linux", target_os = "macos")))]
    {
        let _ = (src, dst);
        Err(conflict("atomic no-replace rename unsupported"))
    }
    #[cfg(any(target_os = "linux", target_os = "macos"))]
    if rc == 0 {
        Ok(())
    } else {
        Err(io::Error::last_os_error())
    }
}

#[cfg(windows)]
/// 原子移动 source 到 target，overwrite 控制替换，不允许 Windows 自动跨卷复制。
fn native_rename(source: &Path, target: &Path, overwrite: bool) -> io::Result<()> {
    use std::os::windows::ffi::OsStrExt;
    use windows_sys::Win32::Storage::FileSystem::MoveFileExW;
    let src: Vec<u16> = source.as_os_str().encode_wide().chain(Some(0)).collect();
    let dst: Vec<u16> = target.as_os_str().encode_wide().chain(Some(0)).collect();
    // 两个缓冲区均以 NUL 结尾且存活到调用返回，不使用 COPY_ALLOWED。
    if unsafe { MoveFileExW(src.as_ptr(), dst.as_ptr(), u32::from(overwrite)) } != 0 {
        Ok(())
    } else {
        Err(io::Error::last_os_error())
    }
}

#[cfg(not(any(unix, windows)))]
/// 在没有安全原子命名的平台拒绝 source/target/overwrite，不修改路径。
fn native_rename(_source: &Path, _target: &Path, _overwrite: bool) -> io::Result<()> {
    Err(conflict("atomic move unsupported"))
}

#[cfg(windows)]
/// 获取 path 的卷/文件 ID，不跟随 reparse point；返回用于拒绝硬链接别名的三元组。
fn windows_identity(path: &Path) -> io::Result<(u32, u32, u32)> {
    use std::os::windows::{fs::OpenOptionsExt, io::AsRawHandle};
    use windows_sys::Win32::Storage::FileSystem::{
        GetFileInformationByHandle, BY_HANDLE_FILE_INFORMATION,
    };
    let f = OpenOptions::new()
        .access_mode(0)
        .custom_flags(0x02200000)
        .open(path)?;
    let mut info = std::mem::MaybeUninit::<BY_HANDLE_FILE_INFORMATION>::uninit();
    // f 维持句柄有效；系统成功后才读取完整初始化的结构。
    if unsafe { GetFileInformationByHandle(f.as_raw_handle(), info.as_mut_ptr()) } == 0 {
        return Err(io::Error::last_os_error());
    }
    let info = unsafe { info.assume_init() };
    Ok((
        info.dwVolumeSerialNumber,
        info.nFileIndexHigh,
        info.nFileIndexLow,
    ))
}

/// 移动 source 到最终 target；overwrite 默认为调用方显式传 false，true 按原生规则替换。
///
/// # Arguments
/// source 是已存在普通文件/目录/链接；target 的父目录必须存在，不合并目录。
/// 调用方必须独占源树、祖先目录及目标父目录；同路径和硬链接别名拒绝。
/// # Returns
/// 已发布的 MoveResult，目录同步失败优先报告 DurabilityUncertain；发布后不能盲目重试。
/// # Errors
/// 发布前返回 io::Error，源及旧目标保持，临时产物尽力清理。只有 EXDEV 才复制降级。
/// 跨卷非端到端原子；仅保留普通 rwx、mtime、链接文本，不保证 uid/gid、ACL、xattr、
/// 稀疏布局、硬链接拓扑或并发写入安全。Windows 仅支持同卷，目录同步可能不支持。
pub fn move_file(
    source: impl AsRef<Path>,
    target: impl AsRef<Path>,
    overwrite: bool,
) -> io::Result<MoveResult> {
    move_with(source.as_ref(), target.as_ref(), overwrite, &NativeIo)
}

/// 用 ops 执行 source 到 target 的阶段协议；overwrite 控制最终排他发布。
/// 发布前错误外传，发布后仅返回 MoveResult，保证调用者能判断目标已经可见。
fn move_with(
    source: &Path,
    target: &Path,
    overwrite: bool,
    ops: &impl MoveIo,
) -> io::Result<MoveResult> {
    let source = valid_path(source)?;
    let target = valid_path(target)?;
    let info = fs::symlink_metadata(source)?;
    if !info.is_file() && !info.is_dir() && !info.is_symlink() {
        return Err(conflict("unsupported source type"));
    }
    match fs::symlink_metadata(target) {
        Ok(other) => {
            #[cfg(unix)]
            if same_identity(&info, &other) {
                return Err(conflict("source and target are aliases"));
            }
            #[cfg(windows)]
            {
                let _ = other;
                if windows_identity(source)? == windows_identity(target)? {
                    return Err(conflict("source and target are aliases"));
                }
            }
        }
        Err(e) if e.kind() == io::ErrorKind::NotFound => (),
        Err(e) => return Err(e),
    }
    if info.is_dir() {
        let root = fs::canonicalize(source)?;
        if fs::canonicalize(parent(target))?.starts_with(root) {
            return Err(conflict("target is inside source"));
        }
    }
    match ops.rename(source, target, overwrite) {
        Ok(()) => {
            // 竞争者可把目标变成源硬链接；覆盖 rename 的成功不必然表示源已删除。
            let source_error = match fs::symlink_metadata(source) {
                Ok(_) => Some(conflict("source remains after rename")),
                Err(e) if e.kind() == io::ErrorKind::NotFound => None,
                Err(e) => Some(e),
            };
            let removed = source_error.is_none();
            let error = ops
                .sync_dir(parent(target))
                .and_then(|()| ops.sync_dir(parent(source)))
                .err();
            return Ok(MoveResult {
                status: if error.is_some() {
                    MoveStatus::DurabilityUncertain
                } else if removed {
                    MoveStatus::Completed
                } else {
                    MoveStatus::PublishedSourceRemains
                },
                source_removed: removed,
                error: error.or(source_error),
            });
        }
        #[cfg(unix)]
        Err(e) if e.raw_os_error() == Some(libc::EXDEV) => (),
        #[cfg(windows)]
        Err(e) if e.raw_os_error() == Some(17) => (),
        Err(e) => return Err(e),
    }
    if !cfg!(unix) {
        return Err(conflict("cross-volume move unsupported on this platform"));
    }
    let stage = Stage::create(parent(target))?;
    let payload = stage.0.join("payload");
    let mut entries = Vec::new();
    copy_tree(source, &payload, &mut entries, ops)?;
    for entry in &entries {
        unchanged(entry, false)?;
    }
    ops.sync_dir(&stage.0)?;
    ops.sync_dir(parent(target))?;
    ops.rename(&payload, target, overwrite)?;
    // 从此目标已发布，任何普通 I/O 错误都转换为带阶段的结果。
    if let Err(error) = fs::remove_dir(&stage.0).and_then(|()| ops.sync_dir(parent(target))) {
        return Ok(MoveResult {
            status: MoveStatus::DurabilityUncertain,
            source_removed: false,
            error: Some(error),
        });
    }
    for entry in &entries {
        if let Err(error) = unchanged(entry, false) {
            return Ok(MoveResult {
                status: MoveStatus::PublishedSourceRemains,
                source_removed: false,
                error: Some(error),
            });
        }
    }
    for (i, entry) in entries.iter().enumerate().rev() {
        // 全树已复核；删除硬链接兄弟会改变 ctime，此处只复核身份。
        if let Err(error) = unchanged(entry, true).and_then(|()| ops.remove(entry)) {
            return Ok(MoveResult {
                status: MoveStatus::PublishedSourceRemains,
                source_removed: false,
                error: Some(error),
            });
        }
        if let Err(error) = ops.sync_dir(parent(&entry.path)) {
            return Ok(MoveResult {
                status: MoveStatus::DurabilityUncertain,
                source_removed: i == 0,
                error: Some(error),
            });
        }
    }
    Ok(MoveResult {
        status: MoveStatus::Completed,
        source_removed: true,
        error: None,
    })
}

#[cfg(all(test, unix))]
mod tests {
    use super::*;
    use std::cell::Cell;
    use std::os::unix::fs::{symlink, MetadataExt, PermissionsExt};

    /// 最小完整用法：准备并关闭源、调用公开 API、检查结果及路径；guard 自动回收资源。
    #[test]
    fn minimal_complete_usage() -> io::Result<()> {
        let root = Stage::create(&std::env::temp_dir())?;
        let source = root.0.join("source");
        let target = root.0.join("target");
        // 源写入完成后文件自动关闭，移动期间不持有写入者。
        fs::write(&source, b"hello")?;
        let result = move_file(&source, &target, false)?;
        // 同时验证公开状态、目标内容与源清理，退出时 Stage 回收目录。
        assert_eq!(result.status, MoveStatus::Completed);
        assert!(result.source_removed && result.error.is_none());
        assert_eq!(fs::read(&target)?, b"hello");
        assert!(!source.exists());
        Ok(())
    }

    /// 为 source 创建 cross 指定的同卷/真实跨卷目标；缺少第二挂载时返回 None 并明确记录。
    fn destination(source: &Path, cross: bool) -> io::Result<Option<Stage>> {
        let parent = if cross { Path::new("/dev/shm") } else { source };
        if cross && (!parent.exists() || fs::metadata(parent)?.dev() == fs::metadata(source)?.dev())
        {
            eprintln!("SKIP: second filesystem unavailable");
            return Ok(None);
        }
        Ok(Some(Stage::create(parent)?))
    }

    /// 验证两种卷模式的空/大文件、普通权限、mtime、目录和不跟随的悬空链接。
    #[test]
    fn real_files_trees_and_metadata() -> io::Result<()> {
        for cross in [false, true] {
            let root = Stage::create(&std::env::temp_dir())?;
            let Some(dest) = destination(&root.0, cross)? else {
                continue;
            };
            for size in [0, 17, 2 * 1024 * 1024 + 137] {
                let source = root.0.join("source");
                let target = dest.0.join("target");
                let data: Vec<u8> = (0..size).map(|n| (n % 251) as u8).collect();
                fs::write(&source, &data)?;
                fs::set_permissions(&source, fs::Permissions::from_mode(0o640))?;
                let stamp =
                    std::time::UNIX_EPOCH + std::time::Duration::new(1_600_000_000, 123456789);
                File::options()
                    .write(true)
                    .open(&source)?
                    .set_times(FileTimes::new().set_modified(stamp))?;
                assert_eq!(
                    move_file(&source, &target, false)?.status,
                    MoveStatus::Completed
                );
                assert_eq!(fs::read(&target)?, data);
                assert_eq!(fs::metadata(&target)?.mode() & 0o777, 0o640);
                assert_eq!(fs::metadata(&target)?.modified()?, stamp);
                assert!(!source.exists());
                fs::remove_file(target)?;
            }
            let source = root.0.join("tree");
            let target = dest.0.join("tree-target");
            fs::create_dir_all(source.join("empty"))?;
            fs::write(source.join("data"), b"tree")?;
            fs::hard_link(source.join("data"), source.join("alias"))?;
            symlink("missing", source.join("link"))?;
            assert_eq!(
                move_file(&source, &target, false)?.status,
                MoveStatus::Completed
            );
            assert!(target.join("empty").is_dir());
            assert_eq!(fs::read(target.join("data"))?, b"tree");
            assert_eq!(fs::read(target.join("alias"))?, b"tree");
            assert_eq!(fs::read_link(target.join("link"))?, Path::new("missing"));
            symlink("missing-root", root.0.join("link"))?;
            assert_eq!(
                move_file(root.0.join("link"), dest.0.join("moved-link"), false)?.status,
                MoveStatus::Completed
            );
        }
        Ok(())
    }

    /// 验证同/跨卷已有链接、非空目录与空目录替换，旧目标和源在失败时保持。
    #[test]
    fn conflicts_and_overwrite() -> io::Result<()> {
        for cross in [false, true] {
            let root = Stage::create(&std::env::temp_dir())?;
            let Some(dest) = destination(&root.0, cross)? else {
                continue;
            };
            let source = root.0.join("source");
            let target = dest.0.join("target");
            fs::write(&source, b"new")?;
            symlink("missing", &target)?;
            assert!(move_file(&source, &target, false).is_err());
            assert_eq!(fs::read(&source)?, b"new");
            assert!(fs::symlink_metadata(&target)?.is_symlink());
            assert_eq!(
                move_file(&source, &target, true)?.status,
                MoveStatus::Completed
            );
            assert_eq!(fs::read(&target)?, b"new");
            fs::remove_file(&target)?;
            fs::create_dir(&source)?;
            fs::create_dir(&target)?;
            fs::write(target.join("old"), b"old")?;
            fs::write(source.join("new"), b"new")?;
            fs::set_permissions(&source, fs::Permissions::from_mode(0o500))?;
            assert!(move_file(&source, &target, true).is_err());
            fs::set_permissions(&source, fs::Permissions::from_mode(0o700))?;
            assert!(!fs::read_dir(&dest.0)?.any(|e| e
                .unwrap()
                .file_name()
                .to_string_lossy()
                .starts_with(".move-")));
            assert_eq!(fs::read(target.join("old"))?, b"old");
            fs::remove_file(target.join("old"))?;
            assert_eq!(
                move_file(&source, &target, true)?.status,
                MoveStatus::Completed
            );
        }
        Ok(())
    }

    /// 同路径、硬链接、目录后代和缺失目标父目录全部拒绝且源保持。
    #[test]
    fn rejects_aliases_and_missing_parent() -> io::Result<()> {
        let root = Stage::create(&std::env::temp_dir())?;
        let source = root.0.join("source");
        let alias = root.0.join("alias");
        fs::write(&source, b"safe")?;
        fs::hard_link(&source, &alias)?;
        for target in [&source, &alias, &root.0.join("missing/target")] {
            assert!(move_file(&source, target, true).is_err());
        }
        let tree = root.0.join("tree");
        fs::create_dir(&tree)?;
        assert!(move_file(&tree, tree.join("child"), false).is_err());
        assert_eq!(fs::read(&source)?, b"safe");
        Ok(())
    }

    /// 每个故障场景独占的 I/O 依赖；published 标记只供当前线程的同步注入使用。
    struct FaultIo {
        source: PathBuf,
        phase: &'static str,
        published: Cell<bool>,
    }

    impl MoveIo for FaultIo {
        /// 首次 source 产生 EXDEV，发布 target 时仍调用真实原子命名，overwrite 原样传递。
        fn rename(&self, source: &Path, target: &Path, overwrite: bool) -> io::Result<()> {
            if self.phase == "alias-race" {
                fs::hard_link(source, target)?;
                return native_rename(source, target, overwrite);
            }
            if self.phase == "native-sync" {
                return native_rename(source, target, overwrite);
            }
            if source == self.source {
                return Err(io::Error::from_raw_os_error(
                    if self.phase == "rename-denied" {
                        libc::EACCES
                    } else {
                        libc::EXDEV
                    },
                ));
            }
            if self.phase == "race" {
                fs::write(target, b"competitor")?;
            }
            native_rename(source, target, overwrite)?;
            self.published.set(true);
            Ok(())
        }
        /// 按阶段拒绝 path 目录同步，其余操作保留真实 fsync。
        fn sync_dir(&self, path: &Path) -> io::Result<()> {
            if self.phase == "native-sync" {
                return Err(conflict("injected directory sync failure"));
            }
            if self.published.get()
                && (self.phase == "publish-sync"
                    || (self.phase == "source-sync" && !self.source.exists()))
            {
                return Err(conflict("injected directory sync failure"));
            }
            NativeIo.sync_dir(path)
        }
        /// entry 是已核验的源项，在 delete 阶段模拟删除权限失败。
        fn remove(&self, entry: &Entry) -> io::Result<()> {
            if self.phase == "delete" {
                return Err(io::Error::from_raw_os_error(libc::EACCES));
            }
            NativeIo.remove(entry)
        }
        /// input/output 为本次流，expected 为原始长度；模拟写满/中断或复制后的源变化。
        fn copy(&self, input: &mut File, output: &mut File, expected: u64) -> io::Result<()> {
            if self.phase == "copy" {
                output.write_all(b"partial")?;
                return Err(io::Error::from_raw_os_error(libc::ENOSPC));
            }
            if self.phase == "interrupt" {
                return Err(io::Error::new(
                    io::ErrorKind::Interrupted,
                    "injected copy interruption",
                ));
            }
            copy_bytes(input, output, expected)?;
            if self.phase == "change" {
                fs::write(&self.source, b"modified-source")?;
            }
            Ok(())
        }
    }

    /// 故障注入覆盖复制、并发目标竞争、源变化、删除失败和发布前后同步失败。
    #[test]
    fn injected_failure_boundaries() -> io::Result<()> {
        for phase in [
            "copy",
            "interrupt",
            "race",
            "change",
            "delete",
            "publish-sync",
            "source-sync",
            "rename-denied",
            "alias-race",
            "native-sync",
        ] {
            let root = Stage::create(&std::env::temp_dir())?;
            let source = root.0.join("source");
            let target = root.0.join("target");
            fs::write(&source, b"original")?;
            let ops = FaultIo {
                source: source.clone(),
                phase,
                published: Cell::new(false),
            };
            let result = move_with(&source, &target, phase == "alias-race", &ops);
            if ["copy", "interrupt", "race", "change", "rename-denied"].contains(&phase) {
                assert!(result.is_err(), "phase {phase}: {result:?}");
                assert!(source.exists());
                if phase == "race" {
                    assert_eq!(fs::read(&target)?, b"competitor");
                } else {
                    assert!(!target.exists());
                }
            } else {
                let result = result?;
                assert_eq!(
                    result.status,
                    if phase == "delete" || phase == "alias-race" {
                        MoveStatus::PublishedSourceRemains
                    } else {
                        MoveStatus::DurabilityUncertain
                    }
                );
                assert_eq!(
                    result.source_removed,
                    phase == "source-sync" || phase == "native-sync"
                );
                assert!(result.error.is_some());
                assert_eq!(fs::read(&target)?, b"original");
            }
            assert!(!fs::read_dir(&root.0)?.any(|e| e
                .unwrap()
                .file_name()
                .to_string_lossy()
                .starts_with(".move-")));
        }
        Ok(())
    }

    /// 删除一个子项后失败，验证目标完整而源树只清理一部分。
    #[test]
    fn partial_directory_cleanup() -> io::Result<()> {
        /// 当前测试的源根和删除计数，不与其他场景共享。
        struct PartialIo {
            source: PathBuf,
            count: Cell<u32>,
        }
        impl MoveIo for PartialIo {
            /// 首次 source 模拟 EXDEV，其他 target/overwrite 使用真实原子发布。
            fn rename(&self, source: &Path, target: &Path, overwrite: bool) -> io::Result<()> {
                if source == self.source {
                    return Err(io::Error::from_raw_os_error(libc::EXDEV));
                }
                native_rename(source, target, overwrite)
            }
            /// 第一次真实删除 entry，第二次报权限失败，保留剩余源树。
            fn remove(&self, entry: &Entry) -> io::Result<()> {
                self.count.set(self.count.get() + 1);
                if self.count.get() == 2 {
                    return Err(io::Error::from_raw_os_error(libc::EACCES));
                }
                NativeIo.remove(entry)
            }
        }
        let root = Stage::create(&std::env::temp_dir())?;
        let source = root.0.join("tree");
        let target = root.0.join("target");
        fs::create_dir(&source)?;
        fs::write(source.join("a"), b"a")?;
        fs::write(source.join("b"), b"b")?;
        let result = move_with(
            &source,
            &target,
            false,
            &PartialIo {
                source: source.clone(),
                count: Cell::new(0),
            },
        )?;
        assert_eq!(result.status, MoveStatus::PublishedSourceRemains);
        assert!(!result.source_removed);
        assert_eq!(fs::read(target.join("a"))?, b"a");
        assert_eq!(fs::read(target.join("b"))?, b"b");
        assert_eq!(fs::read_dir(source)?.count(), 1);
        Ok(())
    }

    /// 特殊类型在复制/阻塞读取前被拒绝，FIFO 仍保留。
    #[test]
    fn rejects_fifo() -> io::Result<()> {
        use std::os::unix::ffi::OsStrExt;
        let root = Stage::create(&std::env::temp_dir())?;
        let source = root.0.join("fifo");
        let name = std::ffi::CString::new(source.as_os_str().as_bytes())?;
        // name 在调用期间有效，0600 只允许测试用户访问。
        assert_eq!(unsafe { libc::mkfifo(name.as_ptr(), 0o600) }, 0);
        assert!(move_file(&source, root.0.join("target"), false).is_err());
        assert!(source.exists());
        Ok(())
    }
}
