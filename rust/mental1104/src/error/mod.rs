//! 统一错误定义（后续模块可复用）
use thiserror::Error;

#[derive(Debug, Error)]
pub enum MentalError {
    #[error("invalid input: {0}")]
    InvalidInput(String),

    #[error("not found")]
    NotFound,
}

pub type Result<T> = std::result::Result<T, MentalError>;
