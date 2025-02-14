fn main() {
    println!("cargo:rustc-link-search=C:/workspace/LLVM/lib");
    println!("cargo:rustc-link-lib=LLVM-C");
}
