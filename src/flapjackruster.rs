#![crate_type = "dylib"]

extern crate libc;
use libc::{c_char, c_int, c_ulong, c_void};
use std::ffi::{CString};

extern crate redis;

#[allow(non_camel_case_types, dead_code, non_snake_case, unused_variables, non_upper_case_globals)]
mod nagios;

// rust-bindgen doesn't seem to translate defined
const NEBMODULE_MODINFO_TITLE:     c_int = 0;
const NEBMODULE_MODINFO_AUTHOR:    c_int = 1;
const NEBMODULE_MODINFO_COPYRIGHT: c_int = 2;
const NEBMODULE_MODINFO_VERSION:   c_int = 3;
const NEBMODULE_MODINFO_LICENSE:   c_int = 4;
const NEBMODULE_MODINFO_DESC:      c_int = 5;

const NSLOG_INFO_MESSAGE: c_ulong = 262144;

const CURRENT_NEB_API_VERSION: c_int = 3;

#[allow(non_upper_case_globals)]
#[no_mangle]
pub static __neb_api_version: c_int = CURRENT_NEB_API_VERSION;

// int nebmodule_init(int flags, char *args, nebmodule *handle)
#[allow(unused_variables)]
#[no_mangle]
pub extern "C" fn nebmodule_init(flags: c_int, args: *const c_char, handle: *mut nagios::nebmodule ) -> c_int {

  // store the handle as the void ptr Nagios expects
  let mut hand = &handle;
  let npcdmod_module_handle: *mut c_void = &mut hand as *mut _ as *mut c_void;

  let title       = CString::new("flapjackruster").unwrap();
  let author      = CString::new("Ali Graham").unwrap();
  let copyright   = CString::new("(c) 2015 Ali Graham").unwrap();
  let version     = CString::new("0.0.1").unwrap();
  let license     = CString::new("GPLv2").unwrap();
  let description = CString::new("A simple performance data / check result extractor / pipe writer.").unwrap();

  let announcement = CString::new("flapjackruster in the house!").unwrap();

  unsafe {
    nagios::neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_TITLE,
      title.as_ptr() as *mut c_char);

    nagios::neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_AUTHOR,
      author.as_ptr() as *mut c_char);

    nagios::neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_COPYRIGHT,
      copyright.as_ptr() as *mut c_char);

    nagios::neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_VERSION,
      version.as_ptr() as *mut c_char);

    nagios::neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_LICENSE,
      license.as_ptr() as *mut c_char);

    nagios::neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_DESC,
      description.as_ptr() as *mut c_char);

    nagios::write_to_all_logs(announcement.as_ptr() as *mut c_char, NSLOG_INFO_MESSAGE);
  }

  return 0;
}

// int nebmodule_deinit(int flags, int reason)
#[allow(unused_variables)]
#[no_mangle]
pub extern "C" fn nebmodule_deinit(flags: c_int, reason: c_int) -> c_int {

  let announcement = CString::new("flapjackruster has left the building.").unwrap();

  unsafe {
    nagios::write_to_all_logs(announcement.as_ptr() as *mut c_char, NSLOG_INFO_MESSAGE);
  }

  return 0;
}

// TODO handle as callback
// void npcdmod_file_roller()
#[allow(unused_variables)]
#[no_mangle]
pub extern "C" fn npcdmod_file_roller() {

}

// TODO handle as callback
/* handle data from Nagios daemon */
// int npcdmod_handle_data(int event_type, void *data) {
#[allow(unused_variables)]
#[no_mangle]
pub extern "C" fn npcdmod_handle_data(event_type: c_int, data: *mut c_void) -> c_int {


  return 0;
}



// Ran into cyclic dependency errors trying to add the PPA to Puppet setup, thus
// installing inside the VM:

// sudo apt-get install software-properties-common
// sudo add-apt-repository ppa:hansjorg/rust
// sudo apt-get update
// sudo apt-get install rust-stable cargo-nightly


// # to rebuild the headers:

// sudo apt-get install clang libclang3.4-dev git
// git clone git@github.com:crabtw/rust-bindgen.git
// cd rust-bindgen
// LIBCLANG_PATH=/usr/lib/llvm-3.4/lib/ cargo build
// target/debug/bindgen -builtins -o ../flapjackfeeder/src/nagios_ub.rs ../flapjackfeeder/include-rust/neb_all.h

