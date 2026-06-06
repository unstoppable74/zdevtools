use std::cmp;
use std::fs;
use std::io::{self, BufRead, BufReader};
use std::path;
use std::process;

use anyhow::Context;
use clap::Parser;

mod dis;
mod sources;
mod standard;
mod visual;

use crate::standard::Printer;
use crate::visual::VisualPrinter;

#[derive(Debug, Clone, Parser)]
#[clap(name = "zd", about = "Z-machine disassembler", version = clap::crate_version!(), disable_version_flag(true))]
struct Options {
    /// Supply an unpacked routine address to start disassembling at
    #[clap(short, long, number_of_values(1))]
    address: Vec<String>,

    /// Supply a file from which addresses (as passed to -a) are read, one per line
    #[clap(short = 'A', long)]
    address_file: Option<path::PathBuf>,

    /// Instead of expecting a filename, expect a hex string representing instructions
    #[clap(short, long, conflicts_with = "address", conflicts_with = "address_file")]
    bytestring: Option<String>,

    /// In non-visual mode, display the constituent bytes of all instructions save immediate strings
    #[clap(short, long, conflicts_with = "visual")]
    dumphex: bool,

    /// When using -d mode, also include immediate strings
    #[clap(short = 'D', long, conflicts_with = "visual")]
    dumpstring: bool,

    /// Ignore errors in disassembly where possible
    #[clap(short, long)]
    ignore_errors: bool,

    /// Instead of printing a literal newline when displaying story text, print a ^ character
    #[clap(short, long)]
    newline_char: bool,

    /// The offset (decimal or, with a leading 0x, hexadecimal) at which decoding starts
    #[clap(long, conflicts_with = "address", conflicts_with = "address_file", value_parser = clap_num::maybe_hex::<usize>)]
    start_offset: Option<usize>,

    /// The maximum number of instructions to decode
    #[clap(long)]
    max_instructions: Option<usize>,

    /// Display disassembly in visual mode, which minutely diagrams instructions
    #[clap(short, long)]
    visual: bool,

    /// Verbose: display instructions as they are being disassembled
    #[clap(short = 'V', long)]
    verbose: bool,

    /// Print version information
    #[clap(long = "version")]
    show_version: bool,

    /// Set the width for -d mode
    #[clap(short, long, conflicts_with = "visual")]
    width: Option<usize>,

    /// Set the Z-machine version for bytestring mode
    #[clap(short, long, conflicts_with = "story")]
    zversion: Option<dis::ZVersion>,

    /// The story file to disassemble
    #[clap(required_unless_present = "bytestring", conflicts_with = "bytestring", required_unless_present = "show_version")]
    story: Option<path::PathBuf>,
}

fn main() -> anyhow::Result<()> {
    let options = Options::parse();

    if options.show_version {
        println!("{} {}", clap::crate_name!(), clap::crate_version!());
        process::exit(0);
    }

    let source: Box<dyn dis::DisassemblySource> = if let Some(ref bytestring) = options.bytestring {
        let re = lazy_regex::regex!(r#"(0x|\s|,)"#);
        let bytestring = re.replace_all(bytestring, "");
        let bytes = hex::decode(&*bytestring).with_context(|| format!("invalid byte string: {bytestring}"))?;
        Box::new(sources::BytesSource::new(&bytes, options.zversion.unwrap_or(dis::ZVersion::V5)))
    } else if let Some(ref path) = options.story {
        Box::new(sources::StorySource::new(path).with_context(|| format!("cannot open story file: {}", path.display()))?)
    } else {
        panic!("unreachable");
    };

    let dis_options = dis::Options {
        ignore_errors: options.ignore_errors,
        newline_char: options.newline_char,
        verbose: options.verbose,
        start_offset: options.start_offset,
        max_instructions: options.max_instructions,
    };

    let mut d = dis::Disassembler::new(source, &dis_options);

    let disassembled = d.start(parse_addresses(&options)?)?;

    if options.visual {
        VisualPrinter::new(&disassembled).print()?;
    } else {
        Printer::new(&disassembled,
                     options.dumphex,
                     options.dumpstring,
                     cmp::max(2, options.width.unwrap_or(8))).print();
    }

    Ok(())
}

fn parse_addresses(options: &Options) -> anyhow::Result<Option<Vec<usize>>> {
    let mut address_strings = Vec::new();

    if let Some(file) = &options.address_file {
        let f: Box<dyn io::Read> = match file.to_str() {
            Some("-") => Box::new(io::stdin()),
            _ => Box::new(fs::File::open(file).with_context(|| format!("cannot open address file: {}", file.display()))?),
        };
        let f = BufReader::new(f);

        let mut new = f
            .lines()
            .collect::<Result<Vec<_>, io::Error>>()?;

        address_strings.append(&mut new);
    }

    address_strings.extend(options.address.iter().cloned());

    if address_strings.is_empty() {
        Ok(None)
    } else {
        Ok(Some(address_strings
             .iter()
             .filter_map(|s| match usize::from_str_radix(s.strip_prefix("0x").unwrap_or(s), 16) {
                 Ok(val) => Some(val),
                 Err(e) => {
                     eprintln!("cannot parse address {s}: {e}");
                     None
                 }
             })
             .collect()))
    }
}
