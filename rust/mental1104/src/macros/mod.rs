//! 放置通用宏（目前占位）
#[macro_export]
macro_rules! bail_invalid {
    ($($tt:tt)*) => {
        return Err($crate::error::MentalError::InvalidInput(format!($($tt)*)));
    };
}
