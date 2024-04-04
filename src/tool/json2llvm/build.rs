fn main() {
    println!("cargo:rustc-link-search=C:/workspace/LLVM17/lib");
    println!("cargo:rustc-link-lib=LLVM-C");
}
