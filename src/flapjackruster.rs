#![crate_type = "dylib"]

extern crate libc;
use libc::{c_char, c_int, c_ulong, c_void};
use std::ffi::{CString, CStr};
use std::collections::{HashMap};

extern crate redis;
use redis::Commands;

#[allow(non_camel_case_types, dead_code, non_snake_case, unused_variables, non_upper_case_globals)]
mod nagios;

// rust-bindgen doesn't seem to translate defined constants
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

  let title       = CString::new("flapjackruster").unwrap().as_ptr() as *mut c_char;
  let author      = CString::new("Ali Graham").unwrap().as_ptr() as *mut c_char;
  let copyright   = CString::new("(c) 2015 Ali Graham").unwrap().as_ptr() as *mut c_char;
  let version     = CString::new("0.0.1").unwrap().as_ptr() as *mut c_char;
  let license     = CString::new("GPLv2").unwrap().as_ptr() as *mut c_char;
  let description = CString::new("A simple performance data / check result extractor / pipe writer.").unwrap().as_ptr() as *mut c_char;

  let announcement = CString::new("flapjackruster in the house!").unwrap().as_ptr() as *mut c_char;

  unsafe {
    nagios::neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_TITLE,
      title);

    nagios::neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_AUTHOR,
      author);

    nagios::neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_COPYRIGHT,
      copyright);

    nagios::neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_VERSION,
      version);

    nagios::neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_LICENSE,
      license);

    nagios::neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_DESC,
      description);

    nagios::write_to_all_logs(announcement, NSLOG_INFO_MESSAGE);
  }

  // FIXME panics if args is empty
  let kv_args = process_module_args( str_from_c_str(args) );

  let mut redis_host = &"localhost";
  let mut redis_port = &"6379";

  if kv_args.contains_key(&"redis_host") {
    redis_host = kv_args.get(&"redis_host").unwrap();
  }

  if kv_args.contains_key(&"redis_port") {
    redis_port = kv_args.get(&"redis_port").unwrap();
  }

  let mut redis_address = String::new();
  redis_address.push_str("redis://");
  redis_address.push_str(redis_host);
  redis_address.push_str(":");
  redis_address.push_str(redis_port);

  // let address_out = CString::new(redis_address.as_bytes()).unwrap().as_ptr() as *mut c_char;

  // unsafe {
  //   nagios::write_to_all_logs(address_out, NSLOG_INFO_MESSAGE);
  // }

  // let client = try!(redis::Client::open(redis_address));
  // let con = try!(client.get_connection());

  // let _ : () = try!(con.set("rustrustrust", 42));

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

fn str_from_c_str(s: *const c_char) -> &'static str {
  let c_str: &CStr = unsafe { CStr::from_ptr(s) };
  let buf: &[u8] = c_str.to_bytes();
  return std::str::from_utf8(buf).unwrap();
}

// split line by commas, split by equals k=v
fn process_module_args(args: &str) -> HashMap<&str, &str> {
  let v: Vec<&str> = args.split(',').collect();

  let mut kv_args = HashMap::new();

  for a in v.iter() {
    let av: Vec<&str> = a.splitn(2, '=').collect();
    kv_args.insert(av[0], av[1]);
  }

  return kv_args;
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

