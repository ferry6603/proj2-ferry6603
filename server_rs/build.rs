use std::{env, path::PathBuf};

fn main() {
    let manifest_dir =
        PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("missing CARGO_MANIFEST_DIR"));
    let bin_dir = manifest_dir
        .join("../project2-bin")
        .canonicalize()
        .expect("failed to resolve engine path");

    println!("cargo:rerun-if-changed=build.rs");

    // Link against engine
    println!("cargo:rustc-link-search=native={}", bin_dir.display());
    println!("cargo:rustc-link-lib=static=engine");
}
