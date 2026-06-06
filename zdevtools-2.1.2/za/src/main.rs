#[macro_use]
extern crate lalrpop_util;

use std::fs;
use std::io::{self, BufRead};
use std::path;

use clap::Parser;

mod asm;

#[derive(Debug, Clone, Parser)]
#[clap(name = "za", about = "Z-machine assembler", version = clap::crate_version!())]
struct Options {
    /// The output file to write to
    #[clap(short, long)]
    output_file: Option<path::PathBuf>,

    /// The release version
    #[clap(short, long)]
    release: Option<u16>,

    /// The serial number
    #[clap(short, long)]
    serial: Option<asm::SerialNumber>,

    /// The Z-machine version to target
    #[clap(short, long)]
    zversion: Option<asm::ZVersion>,

    /// For V5+ programs, preallocate a Unicode translation table near the beginning of the file
    #[clap(short, long)]
    preallocate_unicode_table: bool,

    /// For V5+ programs, print a suggested Unicode table (for use with unicode_table) after assembly
    #[clap(long)]
    suggest_unicode_table: bool,

    /// The assembly source
    source: path::PathBuf,
}

fn main() -> anyhow::Result<()> {
    let args = Options::parse();

    let zversion = args.zversion.unwrap_or(asm::ZVersion::V8);
    let release = args.release.unwrap_or(1);
    let serial = args.serial.unwrap_or_default();
    let output_file = args.output_file.unwrap_or_else(|| path::Path::new(&format!("out.z{}", zversion as usize)).to_path_buf());

    if (args.preallocate_unicode_table || args.suggest_unicode_table) && zversion < asm::ZVersion::V5 {
        anyhow::bail!("unicode tables are only supported in V5+");
    }

    let options = asm::Options::new(zversion, release, serial, args.preallocate_unicode_table, args.suggest_unicode_table);

    let mut a = asm::Assembler::new(&args.source, output_file, options)?;

    let f = fs::File::open(&args.source)?;
    let f = io::BufReader::new(f);

    let lines = f
        .lines()
        .collect::<Result<Vec<_>, io::Error>>()?;

    a.assemble(&lines)?;

    Ok(())
}
