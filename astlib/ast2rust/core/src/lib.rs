use std::{
    cell::RefCell,
    hash::{Hash, Hasher},
    rc::Rc,
};

pub mod ast;
pub mod eval;
mod test;
pub mod traverse;

#[derive(Debug)]
pub enum PtrUnwrapError {
    ExpiredWeakPtr(&'static str),
    Nullptr(&'static str),
}

impl std::fmt::Display for PtrUnwrapError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        match self {
            PtrUnwrapError::ExpiredWeakPtr(s) => write!(f, "expired weak ptr: {}", s),
            PtrUnwrapError::Nullptr(s) => write!(f, "nullptr: {}", s),
        }
    }
}

impl std::error::Error for PtrUnwrapError {}

pub struct PtrKey<T: ?Sized> {
    ptr: *const T,
    _phantom: std::marker::PhantomData<T>,
}

impl<T: Sized> PtrKey<T> {
    pub fn new(ptr: &Rc<RefCell<T>>) -> Self {
        PtrKey {
            ptr: ptr.as_ptr() as *const T,
            _phantom: std::marker::PhantomData,
        }
    }
}

impl<T: ?Sized> PartialEq for PtrKey<T> {
    fn eq(&self, other: &Self) -> bool {
        std::ptr::eq(self.ptr, other.ptr)
    }
}

impl<T: ?Sized> Eq for PtrKey<T> {}

impl<T: ?Sized> Hash for PtrKey<T> {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.ptr.hash(state);
    }
}
