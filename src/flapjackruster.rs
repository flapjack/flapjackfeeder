#![crate_type = "dylib"]

extern crate libc;
use libc::{c_int, c_char, c_void};

extern crate redis;

#[allow(non_camel_case_types, dead_code, non_snake_case)]
mod nagios;

// int nebmodule_init(int flags, char *args, nebmodule *handle)
#[no_mangle]
pub extern "C" fn nebmodule_init(flags: c_int, args: *const c_char, handle: *const nagios::nebmodule ) -> c_int {


  return 0;
}

// int nebmodule_deinit(int flags, int reason)
#[no_mangle]
pub extern "C" fn nebmodule_deinit(flags: c_int, reason: c_int) -> c_int {


  return 0;
}

// TODO handle as callback
// void npcdmod_file_roller()
#[no_mangle]
pub extern "C" fn npcdmod_file_roller() {

}

// TODO handle as callback
/* handle data from Nagios daemon */
// int npcdmod_handle_data(int event_type, void *data) {
#[no_mangle]
pub extern "C" fn npcdmod_handle_data(event_type: c_int, data: *mut c_void) -> c_int {


  return 0;
}

// Cloned and built https://github.com/crabtw/rust-bindgen
// export DYLD_LIBRARY_PATH=/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/
// bindgen -o include-rs/nagios.rs include/neb_all.h
