
// fn do_something() -> redis::RedisResult<()> {
//     let client = try!(redis::Client::open("redis://127.0.0.1/"));
//     let con = try!(client.get_connection());

//     /* do something here */

//     Ok(())
// }

// #[no_mangle]
// pub extern "C" fn hello(name: *const libc::c_char) {
//     let buf_name = unsafe { CStr::from_ptr(name).to_bytes() };
//     let str_name = String::from_utf8(buf_name.to_vec()).unwrap();
//     println!("Hello {}!", str_name);
// }

