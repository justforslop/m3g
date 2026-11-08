use std::env;
use std::path::PathBuf;

fn main() {
    let out = PathBuf::from(env::var("OUT_DIR").unwrap());
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());

    // 1. Shim TU that instantiates the single-header implementation.
    let shim = out.join("m3g_impl.c");
    std::fs::write(
        &shim,
        "#define M3G_IMPLEMENTATION\n#include \"m3g.h\"\n",
    )
    .unwrap();

    // 2. Compile C.
    let mut build = cc::Build::new();
    build
        .include(&manifest_dir)          // <-- m3g.h lives here
        .file(&shim)
        .warnings(false)
        .flag_if_supported("-std=c99");

    // Adapters are at the crate root, not csrc/src/.
    for f in ["deflate_io_miniz.c", "image_io_stb.c"] {
        let p = manifest_dir.join(f);
        if p.exists() {
            build.file(p);
        }
    }
    build.compile("m3g");

    // 3. bindgen — parse declarations only (no M3G_IMPLEMENTATION!).
    let wrapper = out.join("m3g_wrapper.h");
    std::fs::write(&wrapper, "#include \"m3g.h\"\n").unwrap();

    let bindings = bindgen::Builder::default()
        .header(wrapper.to_str().unwrap())
        .clang_arg(format!("-I{}", manifest_dir.display())) // <-- same include
        .clang_arg("-std=c99")
        .clang_arg("-DM3G_API=")
        .allowlist_function("m3g_.*")
        .allowlist_type("m3g_.*")
        .allowlist_var("M3G_.*")
        .derive_default(false)
        .derive_debug(false)
        .layout_tests(false)
        .wrap_unsafe_ops(true)
        .generate()
        .expect("bindgen failed");

    bindings
        .write_to_file(out.join("bindings.rs"))
        .expect("write bindings failed");

    println!("cargo:rerun-if-changed=m3g.h");
    println!("cargo:rerun-if-changed=deflate_io_miniz.c");
    println!("cargo:rerun-if-changed=image_io_stb.c");
}
