use ast2rust::ast;
extern crate proc_macro;
use proc_macro::TokenStream;
use proc_macro2;
use quote::quote;
use syn::{parse_macro_input, LitStr};

#[proc_macro]
pub fn ptrTo(input: TokenStream) {}

mod test {
    use crate::ast;
    use crate::ast::Program;
    use crate::test;

    #[test]
    fn test_ptrTo() {
        let prog = test::load_test_file();
        let v = 1;
        let prog = match prog {
            ast::Node::Program(p) => p,
            _ => panic!("unexpected node"),
        };
        let p = ptrTo!(prog->struct_type);
        assert_eq!(p, 1);
    }
}
