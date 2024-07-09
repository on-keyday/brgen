use proc_macro::TokenStream;
extern crate proc_macro;
use proc_macro2::TokenStream as TokenStream2;
use quote::quote;
use syn::{
    parse::{Parse, ParseStream},
    parse_macro_input, token,
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
                let s = format!("\"{}\"", target.to_string());
                quote!(#base.borrow().#target.clone().ok_or(anyhow!(#s))?)
            }
            Self::WeakPtrTo(base, target) => {
                let base = base.to_tokens();
                let error_message = format!("\"{}\"", target.to_string());
                quote!(#base.upgrade().ok_or(anyhow!(#error_message))?.borrow().#target.clone().ok_or(anyhow!(#error_message))?)
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
                quote!(#base.upgrade().ok_or(anyhow!(#error_message))?.borrow().#target)
            }
        }
    }
}

impl Parse for PtrTo {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        let ident = input.parse::<proc_macro2::Ident>()?;
        let mut ret = Self::Ident(ident);
        loop {
            if let Ok(_) = input.parse::<token::Dot>() {
                let target = input.parse::<proc_macro2::Ident>()?;
                ret = Self::PtrTo(Box::new(ret), target);
            } else if let Ok(_) = input.parse::<token::RArrow>() {
                let target = input.parse::<proc_macro2::Ident>()?;
                ret = Self::WeakPtrTo(Box::new(ret), target);
            } else {
                break;
            }
        }
        Ok(ret)
    }
}

#[proc_macro]
pub fn ptr(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as PtrTo);
    let tokens = input.to_tokens();
    TokenStream::from(tokens)
}

#[proc_macro]
pub fn ptr_null(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as PtrTo);
    let tokens = input.to_tokens_may_null();
    TokenStream::from(tokens)
}
