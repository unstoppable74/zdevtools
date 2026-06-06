#![allow(clippy::manual_range_contains)]

use std::collections;
use std::convert::{TryFrom, TryInto};
use std::fmt;
use std::fs;
use std::io::{self, Read, Seek, Write};
use std::path;

use positioned_io::WriteAt;

mod parser;

const UNICODE_REPLACEMENT: char = '�';

#[derive(thiserror::Error, Debug)]
pub(crate) enum AssembleErrorMessage {
    #[error("{0}")]
    Message(String),
    #[error("offset too large")]
    OffsetTooLarge,
    #[error("value is too large to fit into 16 bits")]
    OutOfRange16,
    #[error("value is too large to fit into signed 16 bits")]
    OutOfRange16s,
}

#[derive(Debug, thiserror::Error)]
pub(crate) struct AssembleError {
    message: AssembleErrorMessage,
    lineloc: Option<LineLocation>,
    filename: path::PathBuf,
    explanation: Vec<String>
}

impl fmt::Display for AssembleError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.lineloc {
            Some(ref lineloc) => write!(f, "{}:{}: {}\n  {}", self.filename.display(), lineloc.number, self.message, lineloc.line)?,
            None => write!(f, "{}", self.message)?,
        }

        for line in &self.explanation {
            write!(f, "\n{line}")?;
        }

        Ok(())
    }
}

#[derive(thiserror::Error, Debug)]
pub(crate) enum Error {
    #[error(transparent)]
    IO(#[from] io::Error),
    #[error(transparent)]
    Assemble(AssembleError),
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, PartialOrd, Ord)]
pub(crate) enum ZVersion {
    V1 = 1,
    V2 = 2,
    V3 = 3,
    V4 = 4,
    V5 = 5,
    V6 = 6,
    V7 = 7,
    V8 = 8,
}

#[derive(thiserror::Error, Debug)]
pub(crate) enum ConvertVersionError {
    #[error("Z-machine version must be an integer in the range 1-8")]
    InvalidZMachineVersion,
}

impl std::str::FromStr for ZVersion {
    type Err = ConvertVersionError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        ZVersion::try_from(s.parse::<usize>().map_err(|_| ConvertVersionError::InvalidZMachineVersion)?)
    }
}

impl TryFrom<usize> for ZVersion {
    type Error = ConvertVersionError;

    fn try_from(v: usize) -> Result<Self, Self::Error> {
        match v {
            1 => Ok(ZVersion::V1),
            2 => Ok(ZVersion::V2),
            3 => Ok(ZVersion::V3),
            4 => Ok(ZVersion::V4),
            5 => Ok(ZVersion::V5),
            6 => Ok(ZVersion::V6),
            7 => Ok(ZVersion::V7),
            8 => Ok(ZVersion::V8),
            _ => Err(ConvertVersionError::InvalidZMachineVersion),
        }
    }
}

#[derive(Debug, Clone)]
pub(crate) struct SerialNumber([u8; 6]);

impl Default for SerialNumber {
    fn default() -> Self {
        let serial = chrono::Local::now()
            .format("%y%m%d")
            .to_string()
            .as_bytes()
            .try_into()
            .unwrap();

        SerialNumber(serial)
    }
}

#[derive(thiserror::Error, Debug)]
pub(crate) enum ConvertSerialError {
    #[error("serial number must be 6 characters")]
    WrongLength,
}

impl std::str::FromStr for SerialNumber {
    type Err = ConvertSerialError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let array = s.as_bytes()
            .try_into()
            .map_err(|_| ConvertSerialError::WrongLength)?;
        Ok(SerialNumber(array))
    }
}

#[derive(Debug, Clone)]
pub(crate) struct Options {
    zversion: ZVersion,
    release: u16,
    serial: SerialNumber,
    preallocate_unicode_table: bool,
    suggest_unicode_table: bool,
}

impl Options {
    pub(crate) fn new(zversion: ZVersion, release: u16, serial: SerialNumber, preallocate_unicode_table: bool, suggest_unicode_table: bool) -> Self {
        Options {
            zversion,
            release,
            serial,
            preallocate_unicode_table,
            suggest_unicode_table,
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub(crate) enum Variable {
    SP,
    Local(u8),
    Global(u8),
}

#[derive(Debug, Clone)]
pub(crate) enum Arg {
    Omitted,
    Constant(u16),
    Variable { var: Variable, as_constant: bool },
    PackedLabel(String),
    UnpackedLabel(String),
    JumpLabel(String),
}

impl Arg {
    fn make_type(&self) -> u8 {
        match *self {
            Arg::Omitted => 3,
            Arg::Variable { as_constant: false, .. } => 2,
            Arg::Variable { as_constant: true, .. } => 1,
            Arg::Constant(n) if n <= 255 => 1,
            Arg::Constant(_) | Arg::PackedLabel(_) | Arg::UnpackedLabel(_) | Arg::JumpLabel(_) => 0,
        }
    }

    fn write(&self, label_targets: &mut LabelTargets, offset: usize) -> Vec<u8> {
        match *self {
            Arg::Omitted => vec![],
            Arg::Variable { var: Variable::SP, .. } => vec![0],
            Arg::Variable { var: Variable::Local(n), .. } => vec![n + 1],
            Arg::Variable { var: Variable::Global(n), .. } => vec![n + 0x10],
            Arg::Constant(n) if n <= 255 => vec![n as u8],
            Arg::Constant(n) => vec![(n >> 8) as u8, n as u8],
            Arg::PackedLabel(ref label) => {
                label_targets.add(label, LabelTarget::Packed { from: offset });
                vec![0, 0]
            }
            Arg::UnpackedLabel(ref label) => {
                label_targets.add(label, LabelTarget::Unpacked { from: offset });
                vec![0, 0]
            }
            Arg::JumpLabel(ref label) => {
                label_targets.add(label, LabelTarget::Jump { from: offset });
                vec![0, 0]
            }
        }
    }
}

#[derive(Eq, PartialEq, Hash, Clone, Copy, Debug)]
pub(crate) enum Count {
    Zero,
    One,
    Two,
    Var { double: bool },
    Ext,
}

#[derive(Debug, Copy, Clone)]
pub(crate) enum LabelTarget {
    Jump {
        from: usize,
    },
    Branch {
        from: usize,
        invert: bool,
        small: bool,
    },
    Packed {
        from: usize,
    },
    Unpacked {
        from: usize,
    },
}

#[derive(Debug, Clone)]
pub(crate) struct Opcode {
    count: Count,
    number: u8,
    args: Vec<Arg>,
    branch: Option<parser::Branch>,
    store: Option<Variable>,
    string: Option<String>,
}

impl Opcode {
    pub(crate) fn new(count: Count, number: u8) -> Self {
        Opcode {
            count,
            number,
            args: vec![],
            branch: None,
            store: None,
            string: None,
        }
    }

    pub(crate) fn args(mut self, args: Vec<Arg>) -> Self {
        self.args = args;
        self
    }

    pub(crate) fn branch(mut self, branch: parser::Branch) -> Self {
        self.branch = Some(branch);
        self
    }

    pub(crate) fn store(mut self, var: Variable) -> Self {
        self.store = Some(var);
        self
    }

    pub(crate) fn string(mut self, string: String) -> Self {
        self.string = Some(string);
        self
    }

    fn encode(&self, label_targets: &mut LabelTargets, cur: usize) -> Vec<u8> {
        let mut bytes = match self.count {
            Count::Zero => {
                assert!(self.args.is_empty());
                vec![0xb0 | self.number]
            }
            Count::One => {
                assert!(self.args.len() == 1);
                let n = self.args.get(0).expect("internal error: no arg");
                vec![0x80 | (n.make_type() << 4) | self.number]
            }
            Count::Two if self.args.len() == 2 => {
                let a = self.args.get(0).expect("internal error: no args");
                let b = self.args.get(1).expect("internal error: not enough args");
                let type1 = a.make_type();
                let type2 = b.make_type();

                if type1 == 0 || type2 == 0 {
                    vec![0xc0 | self.number, (type1 << 6) | (type2 << 4) | 0xf]
                } else {
                    vec![((type1 - 1) << 6) | ((type2 - 1) << 5) | self.number]
                }
            }
            Count::Var { .. } | Count::Ext | Count::Two => {
                let mut args = self.args.clone();

                let (mut bytes, double) = match self.count {
                    Count::Var { double } => (vec![0xe0 | self.number], double),
                    Count::Ext => (vec![0xbe, self.number], false),
                    Count::Two => (vec![0xc0 | self.number], false),
                    _ => panic!("internal error: unxpected count"),
                };

                if double {
                    args.resize(8, Arg::Omitted);
                } else {
                    args.resize(4, Arg::Omitted);
                }

                bytes.push(
                    (args[0].make_type() << 6) |
                    (args[1].make_type() << 4) |
                    (args[2].make_type() << 2) |
                     args[3].make_type());

                if double {
                    bytes.push(
                        (args[4].make_type() << 6) |
                        (args[5].make_type() << 4) |
                        (args[6].make_type() << 2) |
                         args[7].make_type());
                }

                bytes
            }
        };

        for arg in &self.args {
            let offset = cur + bytes.len();
            bytes.append(&mut arg.write(label_targets, offset));
        }

        bytes
    }
}

#[derive(Debug, Clone)]
pub(crate) struct LineLocation {
    pub(crate) line: String,
    pub(crate) number: usize,
}

#[derive(Debug, Clone)]
struct Target {
    label: String,
    target: LabelTarget,
    lineloc: Option<LineLocation>,
}

#[derive(Debug, Clone)]
struct LabelTargets {
    targets: Vec<Target>,
    current_lineloc: Option<LineLocation>,
}

impl LabelTargets {
    fn new() -> Self {
        LabelTargets {
            targets: Vec::new(),
            current_lineloc: None,
        }
    }

    fn add(&mut self, label: &str, target: LabelTarget) {
        self.targets.push(Target {
            label: label.to_string(),
            target,
            lineloc: self.current_lineloc.clone(),
        });
    }
}

pub(crate) struct Assembler {
    filename: path::PathBuf,
    lineloc: Option<LineLocation>,

    file: fs::File,
    options: Options,

    started: bool,

    global_addr: u16,

    labels: collections::HashMap<String, u64>,
    label_targets: LabelTargets,

    custom_unicode_table: Option<Vec<char>>,
    preallocated_unicode_table: Option<u16>,
    unicode_table: Vec<char>,
    etable_unicode_addr: Option<u16>,
}

impl Assembler {
    pub(crate) fn new<P, Q>(input: P, outfile: Q, options: Options) -> Result<Assembler, Error>
    where
        P: AsRef<path::Path>,
        Q: AsRef<path::Path>,
    {
        let mut a = Assembler {
            filename: input.as_ref().to_path_buf(),
            lineloc: None,

            file: fs::OpenOptions::new()
                .read(true)
                .write(true)
                .truncate(true)
                .create(true)
                .append(false)
                .open(outfile)?,

            options,

            started: false,

            global_addr: 0,

            labels: collections::HashMap::new(),
            label_targets: LabelTargets::new(),

            unicode_table: Vec::new(),
            preallocated_unicode_table: None,
            custom_unicode_table: None,
            etable_unicode_addr: None,
        };

        Self::start_file(&mut a)?;

        Ok(a)
    }

    pub(crate) fn assemble(&mut self, lines: &[String]) -> Result<(), Error> {
        for (i, line) in lines.iter().enumerate() {
            // For now comments must start at the beginning of a line.
            if line.starts_with('#') || line.is_empty() {
                continue;
            }

            self.lineloc = Some(LineLocation {
                line: line.clone(),
                number: i + 1,
            });

            self.label_targets.current_lineloc = self.lineloc.clone();

            match parser::parse_directive(self.options.zversion, line) {
                Ok(parser::Directive::Instruction(opcode)) => self.handle_opcode(&opcode)?,
                Ok(parser::Directive::Start) => self.start_directive(None)?,
                Ok(parser::Directive::StartV6(args)) => self.start_directive(Some(args))?,
                Ok(parser::Directive::DefaultAlign) => self.default_align_directive()?,
                Ok(parser::Directive::Align { alignment }) => self.align_directive(alignment)?,
                Ok(parser::Directive::Routine { label, nlocals, values }) => self.routine_directive(&label, nlocals as u8, &values)?,
                Ok(parser::Directive::Status(status_type)) => self.status_directive(status_type)?,
                Ok(parser::Directive::Seek { bytes }) => self.seek_directive(bytes)?,
                Ok(parser::Directive::SeekAbs { offset }) => self.seekabs_directive(offset, 0x00)?,
                Ok(parser::Directive::SeekNop { bytes }) => self.seeknop_directive(bytes)?,
                Ok(parser::Directive::SeekNopAbs { offset }) => self.seekabs_directive(offset, 0xb4)?,
                Ok(parser::Directive::Label(label)) => self.label_directive(&label)?,
                Ok(parser::Directive::AlignedLabel(label)) => self.alabel_directive(&label)?,
                Ok(parser::Directive::String(string)) => self.string_directive(&string)?,
                Ok(parser::Directive::Byte(bytes)) => self.byte_directive(&bytes)?,
                Ok(parser::Directive::SetGlobal(global, value)) => self.set_global_directive(global, value)?,
                Ok(parser::Directive::UnicodeTable(table)) => self.unicode_table_directive(&table)?,
                Err(parser::Error::UnrecognizedToken((lo, hi), expected)) => {
                    let mut explanation = vec![format!("  {}{}", " ".repeat(lo), "~".repeat(hi - lo))];
                    if !expected.is_empty() {
                        explanation.push(format!("expected one of: {}", expected.join(", ")));
                    }
                    return Err(self.error_explain("unrecognized token", &explanation));
                }
                Err(parser::Error::UnexpectedEOF(n)) => {
                    return Err(self.error_explain("unexpected eof", &[
                                                  format!("  {}^", " ".repeat(n))]));
                }
                Err(parser::Error::InvalidToken(n)) => {
                    return Err(self.error_explain("invalid token", &[
                                                  format!("  {}^", " ".repeat(n))]));
                }
                Err(e) => return Err(self.error(&format!("unexpected error: {e:?}"))),
            }
        }

        self.end_file()?;

        if !self.started {
            return Err(self.error("no start directive found"));
        }

        Ok(())
    }

    fn start_file(&mut self) -> Result<(), Error> {
        // Zero the header out.
        self.pslice(&[0; 64], 0)?;

        self.pbyte(self.options.zversion as u8, 0x00)?;
        self.pword(self.options.release, 0x02)?;
        self.pslice(&self.options.serial.0.clone(), 0x12)?;

        // Jump past the header.
        self.seek(io::SeekFrom::Start(0x40))?;

        // Header extension table.
        if self.options.zversion >= ZVersion::V5 {
            self.write_position_at(0x36)?;
            self.word(0x0003)?;
            self.word(0x0000)?;
            self.word(0x0000)?;
            self.etable_unicode_addr = Some(self.tell16()?);
            self.word(0x0000)?;
        }

        // Globals.
        self.write_position_at(0x0c)?;
        self.global_addr = self.tell16()?;

        // The first global needs to be set to a valid object for show_status;
        // point it to the default object. In the future, when objects are
        // fully supported, this should be configurable by the user.
        if self.options.zversion <= ZVersion::V3 {
            self.word(0x0001)?;
            self.seek(io::SeekFrom::Current(478))?;
        } else {
            self.seek(io::SeekFrom::Current(480))?;
        }

        if self.options.zversion == ZVersion::V6 || self.options.zversion == ZVersion::V7 {
            self.pword(0x0000, 0x28)?; // Routines offset.
            self.pword(0x0000, 0x2a)?; // Static strings offset.
        }

        // Object table (just one object is created for now).
        self.write_position_at(0x0a)?;

        // Object 1.
        if self.options.zversion <= ZVersion::V3 {
            self.slice(&[0; 62])?; // Property defaults table.
            self.slice(&[0; 4])?;  // Attribute flags.
            self.slice(&[0; 3])?;  // Parent, sibling, child.
        } else {
            self.slice(&[0; 126])?; // Property defaults table.
            self.slice(&[0; 6])?;   // Attribute flags.
            self.slice(&[0; 6])?;   // Parent, sibling, child.
        }

        let properties = self.tell()? + 2;
        self.word(self.to16(properties)?)?;

        // Property table (just the name and a terminating marker).
        self.object_name("Default object")?;
        self.byte(0)?;

        // Preallocate space for the Unicode table (for 97 Unicode characters)
        if self.options.preallocate_unicode_table {
            if let Some(etable_unicode_addr) = self.etable_unicode_addr {
                let addr = self.tell16()?;
                self.preallocated_unicode_table = Some(addr);
                self.pword(addr, u64::from(etable_unicode_addr))?;
                self.slice(&[0; 195])?;
            }
        }

        Ok(())
    }

    fn object_name(&mut self, name: &str) -> Result<(), Error> {
        let words = self.encode_string(name)?;
        let n = words.len() * 2;

        match n.try_into() {
            Ok(n) => {
                self.byte(n)?;
                for w in words {
                    self.word(w)?;
                }
                Ok(())
            }
            Err(_) => Err(self.error("object name too long")),
        }
    }

    fn encode_string(&mut self, string: &str) -> Result<Vec<u16>, Error> {
        let shiftbase: u8 = if self.options.zversion <= ZVersion::V2 { 1 } else { 3 };

        let mut chars: Vec<u8> = Vec::new();

        for mut c in string.chars() {
            if u32::from(c) > 0xffff {
                self.warning(&format!("too-large unicode character {} (0x{:x}: max is 0xffff): substituting unicode replacement character", c, u32::from(c)));
                c = UNICODE_REPLACEMENT;
            }

            if self.options.zversion == ZVersion::V1 && c == '\n' {
                chars.push(1);
            } else if let Some(pos) = self.atable_index(c) {
                let shift = pos / 26;
                let c = pos % 26;

                if shift != 0 {
                    chars.push(shiftbase + shift);
                }
                chars.push(c + 6);
            } else if c == ' ' {
                chars.push(0);
            } else {
                chars.push(shiftbase + 2);
                chars.push(6);

                if c as usize > 126 {
                    let c = self.unicode_to_zscii(c)?;

                    chars.push(c >> 5);
                    chars.push(c & 0x1f);
                } else {
                    chars.push(c as u8 >> 5);
                    chars.push(c as u8 & 0x1f);
                }
            }
        }

        // Pad.
        while chars.is_empty() || chars.len() % 3 != 0 {
            chars.push(5);
        }

        let mut wordstring = chars
            .chunks(3)
            .map(|chunk| (u16::from(chunk[0]) << 10) | (u16::from(chunk[1]) << 5) | u16::from(chunk[2]))
            .collect::<Vec<_>>();

        // Mark the end.
        let len = wordstring.len();
        wordstring[len - 1] |= 0x8000;

        Ok(wordstring)
    }

    fn atable_index(&self, c: char) -> Option<u8> {
        let atable = if self.options.zversion == ZVersion::V1 {
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ ^0123456789.,!?_#'\"/\\<-:()"
        } else {
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\x00^0123456789.,!?_#'\"/\\-:()"
        };

        atable
            .chars()
            .enumerate()
            .find(|&(i, atable_c)| i != 52 && atable_c == c)
            .map(|(i, _)| i as u8)
    }

    fn unicode_to_zscii(&mut self, c: char) -> Result<u8, Error> {
        Ok(if self.options.zversion < ZVersion::V5 {
            self.v4_unicode_to_zscii(c)?
        } else {
            match self.unicode_table.iter().position(|ch| *ch == c) {
                Some(i) => (i + 155) as u8,
                _ => {
                    self.unicode_table.push(c);
                    if self.unicode_table.len() > 97 {
                        return Err(self.error("too many unicode table entries (max 97)"));
                    }
                    self.unicode_table.len() as u8 + 154
                }
            }
        })
    }

    fn v4_unicode_to_zscii(&self, c: char) -> Result<u8, Error> {
        match c {
            '¡' => Ok(222),
            '£' => Ok(219),
            '«' => Ok(163),
            '»' => Ok(162),
            '¿' => Ok(223),
            'À' => Ok(186),
            'Á' => Ok(175),
            'Â' => Ok(196),
            'Ã' => Ok(208),
            'Ä' => Ok(158),
            'Å' => Ok(202),
            'Æ' => Ok(212),
            'Ç' => Ok(214),
            'È' => Ok(187),
            'É' => Ok(176),
            'Ê' => Ok(197),
            'Ë' => Ok(167),
            'Ì' => Ok(188),
            'Í' => Ok(177),
            'Î' => Ok(198),
            'Ï' => Ok(168),
            'Ð' => Ok(218),
            'Ñ' => Ok(209),
            'Ò' => Ok(189),
            'Ó' => Ok(178),
            'Ô' => Ok(199),
            'Õ' => Ok(210),
            'Ö' => Ok(159),
            'Ø' => Ok(204),
            'Ù' => Ok(190),
            'Ú' => Ok(179),
            'Û' => Ok(200),
            'Ü' => Ok(160),
            'Ý' => Ok(180),
            'Þ' => Ok(217),
            'ß' => Ok(161),
            'à' => Ok(181),
            'á' => Ok(169),
            'â' => Ok(191),
            'ã' => Ok(205),
            'ä' => Ok(155),
            'å' => Ok(201),
            'æ' => Ok(211),
            'ç' => Ok(213),
            'è' => Ok(182),
            'é' => Ok(170),
            'ê' => Ok(192),
            'ë' => Ok(164),
            'ì' => Ok(183),
            'í' => Ok(171),
            'î' => Ok(193),
            'ï' => Ok(165),
            'ð' => Ok(216),
            'ñ' => Ok(206),
            'ò' => Ok(184),
            'ó' => Ok(172),
            'ô' => Ok(194),
            'õ' => Ok(207),
            'ö' => Ok(156),
            'ø' => Ok(203),
            'ù' => Ok(185),
            'ú' => Ok(173),
            'û' => Ok(195),
            'ü' => Ok(157),
            'ý' => Ok(174),
            'þ' => Ok(215),
            'ÿ' => Ok(166),
            'Œ' => Ok(221),
            'œ' => Ok(220),
            _ => Err(self.error(&format!("invalid unicode character for {:?}: {}", self.options.zversion, c))),
        }
    }

    fn handle_opcode(&mut self, opcode: &Opcode) -> Result<(), Error> {
        let cur = self.tell()? as usize;
        let mut bytes = opcode.encode(&mut self.label_targets, cur);

        match opcode.branch {
            Some(parser::Branch::Labeled { ref label, invert, small }) => {
                self.label_targets.add(label, LabelTarget::Branch { from: cur + bytes.len(), invert, small });
                bytes.push(0);

                if !small {
                    bytes.push(0);
                }
            }
            Some(parser::Branch::ReturnTrue { invert: false }) => bytes.push(0xc1),
            Some(parser::Branch::ReturnTrue { invert: true }) => bytes.push(0x41),
            Some(parser::Branch::ReturnFalse { invert: false }) => bytes.push(0xc0),
            Some(parser::Branch::ReturnFalse { invert: true }) => bytes.push(0x40),
            None => (),
        }

        if let Some(ref string) = opcode.string {
            for word in self.encode_string(string)? {
                bytes.push((word >> 8) as u8);
                bytes.push((word & 0xff) as u8);
            }
        }

        match opcode.store {
            Some(Variable::SP) => bytes.push(0),
            Some(Variable::Local(n)) => bytes.push(n + 0x01),
            Some(Variable::Global(n)) => bytes.push(n + 0x10),
            _ => (),
        }

        self.slice(&bytes)?;

        Ok(())
    }

    fn start_directive(&mut self, args: Option<parser::StartV6Args>) -> Result<(), Error> {
        if self.started {
            return Err(self.error("multipe start directives found"));
        }

        self.write_position_at(0x04)?; // base of high memory
        self.write_position_at(0x0e)?; // overlap static and high for now

        // Dictionary.
        self.write_position_at(0x08)?;
        self.byte(0x00)?;
        self.byte(0x06)?;
        self.word(0x0000)?;

        // Packed address of initial routine (V6) or initial PC value (otherwise).
        if self.options.zversion == ZVersion::V6 {
            match args {
                Some(parser::StartV6Args { label, nlocals }) => {
                    self.seekalign()?;
                    let cur = self.tell()?;
                    let packed = self.pack(cur);
                    self.pword(self.to16(packed)?, 0x06)?;
                    self.routine_directive(&label, nlocals as u8, &[])?;
                }
                _ => return Err(self.error("internal error: no V6 args")),
            }
        } else {
            self.write_position_at(0x06)?;
        }

        self.started = true;

        Ok(())
    }

    fn default_align_directive(&mut self) -> Result<(), Error> {
        self.seekalign()?;

        Ok(())
    }

    fn align_directive(&mut self, n: usize) -> Result<(), Error> {
        let cur = self.tell()?;
        let to = self.roundup(cur, n as u64)?;

        self.slice(&vec![0; (to - cur) as usize])?;

        Ok(())
    }

    fn routine_directive(&mut self, label: &str, nlocals: u8, values: &[usize]) -> Result<(), Error> {
        self.seekalign()?;
        self.label_directive(label)?;
        self.byte(nlocals)?;

        let mut values = values.to_owned();

        if nlocals > 15 {
            return Err(self.error("invalid number of locals (must be a number between 0 and 15)"));
        }

        if values.len() > nlocals as usize {
            return Err(self.error("too many local values provided"));
        }

        values.resize(nlocals as usize, 0);

        if self.options.zversion <= ZVersion::V4 {
            for value in values {
                match u16::try_from(value) {
                    Ok(value) => self.word(value)?,
                    Err(_) => return Err(self.error(&format!("invalid local (must be a number between {} and {})", 0, 0xfff))),
                }
            }
        }

        Ok(())
    }

    fn status_directive(&mut self, status_type: parser::StatusType) -> Result<(), Error> {
        match status_type {
            parser::StatusType::Score => self.pbyte(0x00, 0x01)?,
            parser::StatusType::Time => self.pbyte(0x02, 0x01)?,
        }

        Ok(())
    }

    fn seek_directive(&mut self, n: usize) -> Result<(), Error> {
        self.slice(&vec![0; n])?;

        Ok(())
    }

    fn seeknop_directive(&mut self, n: usize) -> Result<(), Error> {
        let buffer = vec![0xb4; n];

        self.slice(&buffer)?;

        Ok(())
    }

    fn seekabs_directive(&mut self, offset: usize, value: u8) -> Result<(), Error> {
        let pos = self.tell()? as usize;
        if offset < pos {
            return Err(self.error(&format!("would seek backward ({offset} < {pos})")));
        }

        let buffer = vec![value; offset - pos];

        self.slice(&buffer)?;
        Ok(())
    }

    fn label_directive(&mut self, label: &str) -> Result<(), Error> {
        let cur = self.tell()?;

        if self.labels.insert(label.to_string(), cur).is_some() {
            return Err(self.error("duplicate label"));
        }

        Ok(())
    }

    fn alabel_directive(&mut self, label: &str) -> Result<(), Error> {
        self.seekalign()?;
        self.label_directive(label)?;

        Ok(())
    }

    fn string_directive(&mut self, string: &str) -> Result<(), Error> {
        let words = self.encode_string(string)?;

        for word in words {
            self.word(word)?;
        }

        Ok(())
    }

    fn byte_directive(&mut self, bytes: &[usize]) -> Result<(), Error> {
        let bytes = bytes
            .iter()
            .map(|b| u8::try_from(*b))
            .collect::<Result<Vec<_>, _>>()
            .map_err(|_| self.error("Bytes must be in the range 0-255"))?;

        self.slice(&bytes)?;

        Ok(())
    }

    fn set_global_directive(&mut self, global: Variable, value: usize) -> Result<(), Error> {
        if self.started {
            return Err(self.error("\"set_global\" must come before \"start\""));
        }

        let value = u16::try_from(value).map_err(|_| self.error("Global variables must be in the range 0-65535"))?;

        match global {
            Variable::Global(global) => {
                let offset = u64::from(self.global_addr) + (2 * u64::from(global));
                self.pword(value, offset)?;
            }
            _ => panic!("internal error: non-global assumed to be global: {global:?}"),
        }

        Ok(())
    }

    fn unicode_table_directive(&mut self, table: &[char]) -> Result<(), Error> {
        if self.custom_unicode_table.is_some() {
            return Err(self.error("multiple unicode_table directives found"));
        }

        self.custom_unicode_table = Some(table.to_vec());

        self.write_unicode_table(table)
    }

    fn end_file(&mut self) -> Result<(), Error> {
        self.apply_label_targets()?;

        self.lineloc = None;

        if self.options.suggest_unicode_table {
            if self.unicode_table.is_empty() {
                println!("No suggested Unicode table, because no Unicode characters were used");
            } else {
                let string_table = self.unicode_table
                    .iter()
                    .collect::<String>();

                let int_table = self.unicode_table
                    .iter()
                    .map(|c| format!("0x{:x}", u32::from(*c)))
                    .collect::<Vec<_>>()
                    .join(" ");

                println!("Suggested Unicode table (both string and integer forms):");
                println!();
                println!("unicode_table \"{string_table}\"");
                println!("unicode_table {int_table}");
            }
        }

        if let Some(ref custom_unicode_table) = self.custom_unicode_table {
            let extra = self.unicode_table
                .iter()
                .filter_map(|c|
                    if custom_unicode_table.contains(c) {
                        None
                    } else {
                        Some(format!("{} (0x{:x})", c, u32::from(*c)))
                    }
                )
                .collect::<Vec<_>>()
                .join(", ");
            if !extra.is_empty() {
                eprintln!("Warning: the following unicode characters were used, but not accounted for in the custom unicode table: {extra}");
            }
        } else {
            self.write_unicode_table(&self.unicode_table.clone())?;
        }

        if self.options.zversion >= ZVersion::V3 {
            let mut buf = Vec::new();

            self.seek(io::SeekFrom::Start(0x40))?;
            self.file.read_to_end(&mut buf)?;

            let file_size = 0x40 + buf.len() as u64;
            let padding = self.align(file_size)? - file_size;
            self.slice(&vec![0; padding as usize])?;

            let file_size = self.align(file_size)?;
            let divisor = match self.options.zversion {
                ZVersion::V1 | ZVersion::V2 | ZVersion::V3 => 2,
                ZVersion::V4 | ZVersion::V5 => 4,
                ZVersion::V6 | ZVersion::V7 | ZVersion::V8 => 8,
            };
            self.pword(self.to16(file_size / divisor)?, 0x1a)?;

            let checksum: u32 = buf
                .into_iter()
                .map(u32::from)
                .sum();

            self.pword(checksum as u16, 0x1c)?;
        }

        Ok(())
    }

    fn apply_label_targets(&mut self) -> Result<(), Error> {
        for Target { label, target, lineloc } in self.label_targets.targets.clone() {
            self.lineloc = lineloc;

            let to = match self.labels.get(&label) {
                Some(to) => *to,
                None => return Err(self.error(&format!("unknown label: {label}"))),
            };

            match target {
                LabelTarget::Jump { from } => {
                    let target = (to as i64) - (from as i64);
                    let offset = self.to16s(target)? as u16;

                    self.pword(offset, from as u64)?;
                }
                LabelTarget::Branch { from, invert, small: true } => {
                    let target = (to as i64) - (from as i64) + 1;

                    #[allow(clippy::manual_range_contains)]
                    if target < 0 || target > 63 {
                        return Err(self.error(&format!("offset ({target}) does not fit into unsigned 6-bit value")));
                    }

                    let mut offset = (target as u8) & 0x3f;

                    offset |= 0x40;

                    if !invert {
                        offset |= 0x80;
                    }

                    self.pbyte(offset, from as u64)?;
                }
                LabelTarget::Branch { from, invert, small: false } => {
                    let target = (to as i64) - (from as i64);

                    if target < -8192 || target > 8191 {
                        return Err(self.error(&format!("offset ({target}) does not fit into signed 14-bit value")));
                    }

                    let mut offset = (target as u16) & 0x3fff;

                    if !invert {
                        offset |= 0x8000;
                    }

                    self.pword(offset, from as u64)?;
                }
                LabelTarget::Packed { from } => {
                    let alignment = self.alignment();

                    if to % alignment != 0 {
                        return Err(self.error("attempt to pack unaligned address"));
                    }

                    self.pword(self.to16(to / alignment)?, from as u64)?;
                }
                LabelTarget::Unpacked { from } => {
                    self.pword(self.to16(to)?, from as u64)?;
                }
            }
        }

        Ok(())
    }

    fn write_unicode_table(&mut self, table: &[char]) -> Result<(), Error> {
        if table.len() > 97 {
            return Err(self.error("unicode table too large (max 97)"));
        }

        if table.iter().any(|c| u32::from(*c) > 0xffff) {
            return Err(self.error("unicode table contains too-large character (max 0xffff)"));
        }

        if let Some(etable_unicode_addr) = self.etable_unicode_addr {
            let len = table.len();
            if len > 0 {
                if let Some(preallocated_unicode_table) = self.preallocated_unicode_table {
                    self.pbyte(len as u8, u64::from(preallocated_unicode_table))?;
                    for (i, w) in table.iter().enumerate() {
                        self.pword(*w as u16, u64::from(preallocated_unicode_table) + 1 + (2 * i as u64))?;
                    }
                } else {
                    self.write_position_at(etable_unicode_addr.into())
                        .map_err(|err| {
                            match err {
                                Error::Assemble(AssembleError {
                                    message: AssembleErrorMessage::OffsetTooLarge,
                                    ..
                                }) => self.error("unicode table out of range (try --preallocate-unicode-table)"),
                                _ => err,
                            }
                        })?;
                    self.byte(len as u8)?;
                    for w in table {
                        self.word(*w as u16)?;
                    }
                }
            }
        }

        Ok(())
    }

    #[must_use]
    fn error(&self, message: &str) -> Error {
        self.error_explain(message, &[])
    }

    #[must_use]
    fn error_class(&self, message: AssembleErrorMessage) -> Error {
        self.error_class_explain(message, &[])
    }

    #[must_use]
    fn error_explain(&self, message: &str, explanation: &[String]) -> Error {
        self.error_class_explain(AssembleErrorMessage::Message(message.into()), explanation)
    }

    #[must_use]
    fn error_class_explain(&self, message: AssembleErrorMessage, explanation: &[String]) -> Error {
        Error::Assemble(AssembleError {
            message,
            lineloc: self.lineloc.clone(),
            filename: self.filename.clone(),
            explanation: explanation.into(),
        })
    }

    fn warning(&self, message: &str) {
        match self.lineloc {
            Some(ref lineloc) => eprintln!("warning: {}:{}: {}", self.filename.display(), lineloc.number, message),
            None => eprintln!("warning: {message}"),
        };
    }

    fn pack(&self, a: u64) -> u64 {
        a / self.alignment()
    }

    fn seekalign(&mut self) -> Result<(), Error> {
        let cur = self.tell()?;
        let aligned = self.align(cur)?;
        self.slice(&vec![0; (aligned - cur) as usize])?;
        Ok(())
    }

    fn align(&self, v: u64) -> Result<u64, Error> {
        self.roundup(v, self.alignment())
    }

    fn roundup(&self, v: u64, multiple: u64) -> Result<u64, Error> {
        if multiple == 0 {
            Err(self.error("cannot align to 0"))
        } else {
            Ok(multiple * (((v - 1) / multiple) + 1))
        }
    }

    fn alignment(&self) -> u64 {
        match self.options.zversion {
            ZVersion::V1 | ZVersion::V2 | ZVersion::V3 => 2,
            ZVersion::V4 | ZVersion::V5 | ZVersion::V6 | ZVersion::V7 => 4,
            ZVersion::V8 => 8,
        }
    }

    fn write_position_at(&mut self, offset: u64) -> Result<(), Error> {
        let cur = u16::try_from(self.tell()?)
            .map_err(|_| self.error_class(AssembleErrorMessage::OffsetTooLarge))?;

        self.pword(cur, offset)?;
        Ok(())
    }

    fn seek(&mut self, offset: io::SeekFrom) -> Result<(), Error> {
        self.file.seek(offset)?;
        Ok(())
    }

    fn tell(&mut self) -> Result<u64, Error> {
        Ok(self.file.stream_position()?)
    }

    fn tell16(&mut self) -> Result<u16, Error> {
        u16::try_from(self.tell()?)
            .map_err(|_| self.error_class(AssembleErrorMessage::OffsetTooLarge))
    }

    fn word(&mut self, val: u16) -> Result<(), Error> {
        self.byte((val >> 8) as u8)?;
        self.byte((val & 0xff) as u8)?;

        Ok(())
    }

    fn byte(&mut self, val: u8) -> Result<(), Error> {
        self.file.write_all(&[val])?;
        Ok(())
    }

    fn slice(&mut self, val: &[u8]) -> Result<(), Error> {
        self.file.write_all(val)?;
        Ok(())
    }

    fn pword(&mut self, val: u16, offset: u64) -> Result<(), Error> {
        self.pbyte((val >> 8) as u8, offset)?;
        self.pbyte((val & 0xff) as u8, offset + 1)?;

        Ok(())
    }

    fn pbyte(&mut self, val: u8, offset: u64) -> Result<(), Error> {
        self.file.write_at(offset, &[val])?;
        Ok(())
    }

    fn pslice(&mut self, val: &[u8], offset: u64) -> Result<(), Error> {
        self.file.write_at(offset, val)?;
        Ok(())
    }

    fn to16<T>(&self, val: T) -> Result<u16, Error>
    where u16: TryFrom<T>
    {
        u16::try_from(val)
            .map_err(|_| self.error_class(AssembleErrorMessage::OutOfRange16))
    }

    fn to16s<T>(&self, val: T) -> Result<i16, Error>
    where i16: TryFrom<T>
    {
        i16::try_from(val)
            .map_err(|_| self.error_class(AssembleErrorMessage::OutOfRange16s))
    }
}
