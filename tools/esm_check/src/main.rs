use esplugin::{GameId, ParseOptions, Plugin};
use std::path::Path;
fn main() {
    let path = std::env::args().nth(1).expect("path to plugin");
    let mut p = Plugin::new(GameId::SkyrimSE, Path::new(&path));
    p.parse_file(ParseOptions::whole_plugin()).expect("read records");
    assert!(p.is_master_file() && p.is_light_plugin());
    assert!(p.is_valid_as_light_plugin().unwrap());
    assert_eq!(p.masters().unwrap(), vec!["Skyrim.esm"]);
    assert_eq!(p.header_version(), Some(1.71));
    assert_eq!(p.record_and_group_count(), Some(36));
    println!("esplugin parsed the full file and accepted its ESM and ESL flags, FormID range, master list, and record and group count.");
}
