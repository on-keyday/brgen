use proc_macro::TokenStream;
extern crate proc_macro;
use proc_macro2::TokenStream as TokenStream2;
use quote::quote;
use syn::{
    parse::{Parse, ParseStream},
    parse_macro_input, Token,
};

enum PtrTo {
    Ident(proc_macro2::Ident),
    PtrTo(Box<PtrTo>, proc_macro2::Ident),
    WeakPtrTo(Box<PtrTo>, proc_macro2::Ident),
}

impl PtrTo {
    // a.b to a.borrow().b.clone().ok_or(anyhow!("error"))?
    fn to_tokens(&self) -> TokenStream2 {
        match self {
            Self::Ident(ident) => quote!(#ident),
            Self::PtrTo(base, target) => {
                let base = base.to_tokens();
                let error_message = format!("\"{}\"", target.to_string());
                quote!(#base.borrow().#target.clone().ok_or_else(||PtrUnwrapError::Nullptr(#error_message))?)
            }
            Self::WeakPtrTo(base, target) => {
                let base = base.to_tokens();
                let error_message = format!("\"{}\"", target.to_string());
                quote!(#base.upgrade().ok_or_else(|| PtrUnwrapError::ExpiredWeakPtr(#error_message))?.borrow().#target.clone().ok_or_else(||PtrUnwrapError::Nullptr(#error_message))?)
            }
        }
    }

    fn to_tokens_may_null(&self) -> TokenStream2 {
        match self {
            Self::Ident(ident) => quote!(#ident),
            Self::PtrTo(base, target) => {
                let base = base.to_tokens();
                quote!(#base.borrow().#target.clone())
            }
            Self::WeakPtrTo(base, target) => {
                let base = base.to_tokens();
                let error_message = format!("\"{}\"", target.to_string());
                quote!(#base.upgrade().ok_or_else(||PtrUnwrapError::ExpiredWeakPtr(#error_message))?.borrow().#target)
            }
        }
    }
}

impl Parse for PtrTo {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        let ident = input.parse::<proc_macro2::Ident>()?;
        let mut ret = Self::Ident(ident);
        loop {
            if let Ok(_) = input.parse::<Token![.]>() {
                let target = input.parse::<proc_macro2::Ident>()?;
                ret = Self::PtrTo(Box::new(ret), target);
            } else if let Ok(_) = input.parse::<Token![->]>() {
                let target = input.parse::<proc_macro2::Ident>()?;
                ret = Self::WeakPtrTo(Box::new(ret), target);
            } else {
                break;
            }
        }
        Ok(ret)
    }
}

/// Convert a chain of field access to a chain of `borrow().clone().ok_or(anyhow!("error"))?`
#[proc_macro]
pub fn ptr(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as PtrTo);
    let tokens = input.to_tokens();
    TokenStream::from(tokens)
}

/// Convert a chain of field access to a chain of `borrow().clone()`
#[proc_macro]
pub fn ptr_null(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as PtrTo);
    let tokens = input.to_tokens_may_null();
    TokenStream::from(tokens)
}
