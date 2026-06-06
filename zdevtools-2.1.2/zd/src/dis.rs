#![allow(clippy::manual_range_patterns)]

use std::collections::{HashMap, HashSet};
use std::convert::TryFrom;
use std::fmt;

#[derive(Debug, thiserror::Error)]
pub(crate) enum Error {
    #[error("invalid start address: {0}")]
    InvalidStart(u16),
    #[error("invalid routine: {0}")]
    InvalidRoutine(usize),
    #[error("attempt to jump outside of memory: pc = 0x{0:x}")]
    InvalidJump(usize),
    #[error("unknown opcode {count} {number} ({number:x}) @0x{pc:x}")]
    UnknownOpcode { count: Count, number: u8, pc: usize },
    #[error("cannot continue")]
    CannotContinue,
    #[error("abbreviation being used recursively")]
    RecursiveAbbreviation,
    #[error("invalid operand value: {0}")]
    InvalidOperandType(u8),
    #[error("packed address would be too large: {0}")]
    PackedAddressTooLarge(usize),
    #[error("end of file (0x{start:x} to 0x{end:x})")]
    EndOfFile { start: usize, end: usize },
}

#[derive(Debug, Clone)]
pub(crate) struct Options {
    pub(crate) ignore_errors: bool,
    pub(crate) newline_char: bool,
    pub(crate) verbose: bool,
    pub(crate) start_offset: Option<usize>,
    pub(crate) max_instructions: Option<usize>,
}

pub(crate) type UnicodeTable = [u16; 97];
pub(crate) const DEFAULT_UNICODE_TABLE: UnicodeTable = [
    0x00e4, 0x00f6, 0x00fc, 0x00c4, 0x00d6, 0x00dc, 0x00df, 0x00bb, 0x00ab,
    0x00eb, 0x00ef, 0x00ff, 0x00cb, 0x00cf, 0x00e1, 0x00e9, 0x00ed, 0x00f3,
    0x00fa, 0x00fd, 0x00c1, 0x00c9, 0x00cd, 0x00d3, 0x00da, 0x00dd, 0x00e0,
    0x00e8, 0x00ec, 0x00f2, 0x00f9, 0x00c0, 0x00c8, 0x00cc, 0x00d2, 0x00d9,
    0x00e2, 0x00ea, 0x00ee, 0x00f4, 0x00fb, 0x00c2, 0x00ca, 0x00ce, 0x00d4,
    0x00db, 0x00e5, 0x00c5, 0x00f8, 0x00d8, 0x00e3, 0x00f1, 0x00f5, 0x00c3,
    0x00d1, 0x00d5, 0x00e6, 0x00c6, 0x00e7, 0x00c7, 0x00fe, 0x00f0, 0x00de,
    0x00d0, 0x00a3, 0x0153, 0x0152, 0x00a1, 0x00bf, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
];

pub(crate) type AlphabetTable = [u8; 78];
pub(crate) const DEFAULT_ATABLE_V1: AlphabetTable = [
    // A0
    0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d,
    0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,

    // A1
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d,
    0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a,

    // A2
    0x20, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2e, 0x2c,
    0x21, 0x3f, 0x5f, 0x23, 0x27, 0x22, 0x2f, 0x5c, 0x3c, 0x2d, 0x3a, 0x28, 0x29,
];
pub(crate) const DEFAULT_ATABLE_V2: AlphabetTable = [
    // A0
    0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d,
    0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,

    // A1
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d,
    0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a,

    // A2
    0x00, 0x0d, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2e,
    0x2c, 0x21, 0x3f, 0x5f, 0x23, 0x27, 0x22, 0x2f, 0x5c, 0x2d, 0x3a, 0x28, 0x29,
];

#[allow(clippy::upper_case_acronyms)]
enum ZSCII {
    Newline = 13,
    Space = 32,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, PartialOrd, Ord)]
pub(crate) enum ZVersion {
    V1,
    V2,
    V3,
    V4,
    V5,
    V6,
    V7,
    V8,
}

impl fmt::Display for ZVersion {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let version = match self {
            ZVersion::V1 => 1,
            ZVersion::V2 => 2,
            ZVersion::V3 => 3,
            ZVersion::V4 => 4,
            ZVersion::V5 => 5,
            ZVersion::V6 => 6,
            ZVersion::V7 => 7,
            ZVersion::V8 => 8,
        };
        write!(f, "{version}")
    }
}

#[derive(thiserror::Error, Debug)]
pub(crate) enum ConvertError {
    #[error(transparent)]
    ParseIntError(#[from] std::num::ParseIntError),
    #[error("invalid z-machine version: {0}")]
    InvalidZMachineVersion(u8),
}

impl std::str::FromStr for ZVersion {
    type Err = ConvertError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        ZVersion::try_from(s.parse::<u8>()?)
    }
}

impl TryFrom<u8> for ZVersion {
    type Error = ConvertError;

    fn try_from(v: u8) -> Result<Self, Self::Error> {
        match v {
            1 => Ok(ZVersion::V1),
            2 => Ok(ZVersion::V2),
            3 => Ok(ZVersion::V3),
            4 => Ok(ZVersion::V4),
            5 => Ok(ZVersion::V5),
            6 => Ok(ZVersion::V6),
            7 => Ok(ZVersion::V7),
            8 => Ok(ZVersion::V8),
            _ => Err(ConvertError::InvalidZMachineVersion(v)),
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub(crate) enum Arg {
    Constant(u16),
    Variable(u8),
    Store(u8),
    Call(u16, usize),
    Jump(u16, usize),
}

impl fmt::Display for Arg {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Arg::Constant(val) => write!(f, "#{val:02x}"),
            Arg::Store(0) => write!(f, "-(SP)"),
            Arg::Variable(0) => write!(f, "(SP)+"),
            Arg::Variable(var) | Arg::Store(var) if *var <= 0x0f => write!(f, "L{:02}", var - 1),
            Arg::Variable(var) | Arg::Store(var) => write!(f, "G{:02x}", var - 0x10),
            Arg::Call(_packed, addr) => write!(f, "#{addr:04x}"),
            Arg::Jump(_offset, to) => write!(f, "#{to:04x}"),
        }
    }
}

#[derive(Debug, Eq, PartialEq, Copy, Clone)]
pub(crate) enum OperandType {
    Large = 0,
    Small = 1,
    Variable = 2,
    Omitted = 3,
}

impl fmt::Display for OperandType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            OperandType::Large => write!(f, "Large constant"),
            OperandType::Small => write!(f, "Small constant"),
            OperandType::Variable => write!(f, "Variable"),
            OperandType::Omitted => write!(f, "Omitted"),
        }
    }
}

impl TryFrom<u8> for OperandType {
    type Error = Error;

    fn try_from(optype: u8) -> Result<Self, Self::Error> {
        match optype {
            0 => Ok(OperandType::Large),
            1 => Ok(OperandType::Small),
            2 => Ok(OperandType::Variable),
            3 => Ok(OperandType::Omitted),
            _ => Err(Error::InvalidOperandType(optype)),
        }
    }
}

#[derive(Debug, Clone)]
pub(crate) struct Routine {
    pub(crate) addr: usize,
    pub(crate) locals: Vec<u16>,
}

pub(crate) trait AsSigned {
    fn as_signed(&self) -> isize;
}

impl AsSigned for u16 {
    fn as_signed(&self) -> isize {
        *self as i16 as isize
    }
}

#[derive(Eq, PartialEq, Hash, Clone, Copy, Debug)]
pub(crate) enum Form {
    Long,
    Short,
    Extended,
    Variable,
}

#[derive(Eq, PartialEq, Hash, Clone, Copy, Debug)]
pub(crate) enum Count {
    Zero,
    One,
    Two,
    Var,
    Ext,
}

impl fmt::Display for Count {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Count::Zero => write!(f, "0OP"),
            Count::One => write!(f, "1OP"),
            Count::Two => write!(f, "2OP"),
            Count::Var => write!(f, "VAR"),
            Count::Ext => write!(f, "EXT"),
        }
    }
}

#[derive(Debug, Clone)]
pub(crate) struct Opcode {
    pub(crate) count: Count,
    number: usize,
    pub(crate) name: String,
    returns: bool,
    pub(crate) string: bool,
    branch: bool,
    jump: bool,
    store: bool,
    printchar: bool,
    punicode: bool,
    pub(crate) paddr: bool,
    pub(crate) ppaddr: bool,
    pub(crate) indirect: bool,
    call: Option<usize>,
}

macro_rules! bool_builder {
    ($name:ident) => {
        fn $name(mut self) -> Self {
            self.$name = true;
            self
        }
    };
}

impl Opcode {
    fn new(count: Count, number: usize, name: &str) -> Self {
        Opcode {
            count,
            number,
            name: name.to_string(),
            returns: false,
            string: false,
            branch: false,
            jump: false,
            store: false,
            printchar: false,
            punicode: false,
            paddr: false,
            ppaddr: false,
            indirect: false,
            call: None,
        }
    }

    bool_builder!(returns);
    bool_builder!(string);
    bool_builder!(branch);
    bool_builder!(jump);
    bool_builder!(store);
    bool_builder!(printchar);
    bool_builder!(punicode);
    bool_builder!(paddr);
    bool_builder!(ppaddr);
    bool_builder!(indirect);

    pub(crate) fn call(mut self, arg: usize) -> Self {
        self.call = Some(arg);
        self
    }
}

#[derive(Debug, Clone, Copy)]
pub(crate) struct Call {
    pub(crate) packed: u16,
    pub(crate) addr: usize,
}

#[derive(Debug, Clone, Copy)]
pub(crate) struct Jump {
    pub(crate) to: usize,
}

#[derive(Debug, Clone, Copy)]
pub(crate) enum BranchSize {
    Short(u8),
    Long(u8, u8),
}

#[derive(Debug, Clone, Copy)]
pub(crate) enum BranchTarget {
    ReturnFalse,
    ReturnTrue,
    Where { offset: isize, address: usize },
}

impl fmt::Display for BranchTarget {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            BranchTarget::ReturnFalse => write!(f, "RFALSE"),
            BranchTarget::ReturnTrue => write!(f, "RTRUE"),
            BranchTarget::Where { address, .. } => write!(f, "{address:x}"),
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub(crate) struct Branch {
    pub(crate) size: BranchSize,
    pub(crate) invert: bool,
    pub(crate) target: BranchTarget,
}

impl fmt::Display for Branch {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let s = if self.invert {
            "FALSE"
        } else {
            "TRUE"
        };

        write!(f, "[{}] {}", s, self.target)
    }
}

#[derive(Debug, Clone)]
pub(crate) enum StringPart {
    ZSCIIStart,
    ZSCIITop,
    ZSCIIBottom(u16, String),
    AbbrevStart(u16),
    AbbrevOmitted,
    Abbrev(u16, String),
    Space(String),
    Newline(String),
    Shift(u16),
    ShiftLock(u16, u16),
    Character(String),
}

impl fmt::Display for StringPart {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            StringPart::ZSCIIStart => write!(f, "Start of 10-bit ZSCII"),
            StringPart::ZSCIITop => write!(f, "Top of ZSCII"),
            StringPart::ZSCIIBottom(c, s) => write!(f, "Bottom of ZSCII: {c} ({s})"),
            StringPart::AbbrevStart(a) => write!(f, "Abbrev start {a}"),
            StringPart::AbbrevOmitted => write!(f, "Abbrev, but no table available"),
            StringPart::Abbrev(c, s) => write!(f, "Abbrev {c} ({s})"),
            StringPart::Space(s) => write!(f, "{s}"),
            StringPart::Newline(n) => write!(f, "{n}"),
            StringPart::Shift(to) => write!(f, "Shift A{to}"),
            StringPart::ShiftLock(from, to) => write!(f, "Shift lock from A{from} to A{to}"),
            StringPart::Character(c) => write!(f, "{c}"),
        }
    }
}

#[derive(Debug, Clone)]
pub(crate) struct ZString {
    pub(crate) string: String,
    pub(crate) parts: Vec<(u16, [StringPart; 3])>,
}

#[derive(Debug, Clone)]
pub(crate) struct Instruction {
    pub(crate) addr: usize,
    pub(crate) nextaddr: usize,
    pub(crate) form: Form,
    pub(crate) full_opcode: Vec<u8>,
    pub(crate) full_instruction: Vec<u8>,
    pub(crate) args: Vec<Arg>,

    pub(crate) opcode: Opcode,

    pub(crate) call: Option<Call>,
    pub(crate) jump: Option<Jump>,
    pub(crate) branch: Option<Branch>,
    pub(crate) storeto: Option<u8>,
    pub(crate) string: Option<ZString>,
    pub(crate) character: Option<String>,
    pub(crate) punicode: Option<char>,
}

impl Instruction {
    fn new(opcode: &Opcode, form: Form) -> Self {
        Instruction {
            addr: 0,
            nextaddr: 0,
            form,
            full_opcode: Vec::new(),
            full_instruction: Vec::new(),
            args: Vec::new(),
            opcode: opcode.clone(),

            call: None,
            jump: None,
            branch: None,
            storeto: None,
            string: None,
            character: None,
            punicode: None,
        }
    }
}

pub(crate) struct Memory {
    memory: Vec<u8>,
}

impl Memory {
    pub(crate) fn new(memory: Vec<u8>) -> Self {
        Memory {
            memory,
        }
    }

    fn size(&self) -> usize {
        self.memory.len()
    }

    pub(crate) fn byte(&self, offset: usize) -> Result<u8, Error> {
        self.memory
            .get(offset)
            .copied()
            .ok_or(Error::EndOfFile { start: offset, end: offset })
    }

    pub(crate) fn word(&self, offset: usize) -> Result<u16, Error> {
        Ok(u16::from(self.byte(offset)?) << 8 | u16::from(self.byte(offset + 1)?))
    }

    pub(crate) fn slice(&self, start: usize, len: usize) -> Result<&[u8], Error> {
        self.memory
            .get(start..(start + len))
            .ok_or(Error::EndOfFile { start, end: start + len })
    }

    pub(crate) fn slice_words(&self, start: usize, n: usize) -> Result<Vec<u16>, Error> {
        (0..n).map(|i| self.word(start + (i * 2))).collect()
    }
}

#[derive(Debug, Clone)]
pub(crate) struct DisassembledStory {
    pub(crate) zversion: ZVersion,
    pub(crate) instructions: Vec<Instruction>,
    pub(crate) routines: HashMap<usize, Routine>,
    pub(crate) main_routine: usize,
}

pub(crate) trait DisassemblySource {
    fn zversion(&self) -> ZVersion;
    fn can_branch(&self) -> bool;
    fn start(&self) -> u16;
    fn memory(&self) -> &Memory;
    fn s_o(&self) -> usize;
    fn r_o(&self) -> usize;
    fn abbr_table(&self) -> Option<usize>;
    fn eof_is_quit(&self) -> bool;

    fn atable(&self) -> AlphabetTable {
        if self.zversion() == ZVersion::V1 {
            DEFAULT_ATABLE_V1
        } else {
            DEFAULT_ATABLE_V2
        }
    }

    fn unicode_table(&self) -> UnicodeTable {
        DEFAULT_UNICODE_TABLE
    }
}

pub(crate) struct Disassembler {
    source: Box<dyn DisassemblySource>,
    zversion: ZVersion,
    pc: usize,
    opcodes: HashMap<(Count, usize), Opcode>,
    options: Options,
}

type DecodedString = (usize, String, Vec<(u16, [StringPart; 3])>);

impl Disassembler {
    pub(crate) fn new(source: Box<dyn DisassemblySource>, options: &Options) -> Self {
        let zversion = source.zversion();

        Disassembler {
            source,
            zversion,
            pc: 0,
            opcodes: Self::setup_opcodes(zversion),
            options: options.clone(),
        }
    }

    fn setup_opcodes(zversion: ZVersion) -> HashMap<(Count, usize), Opcode> {
        let mut opcodes = HashMap::new();

        let mut op = |min: ZVersion, max: ZVersion, opcode: Opcode| {
            let v = if zversion > ZVersion::V6 { ZVersion::V5 } else { zversion };

            if v >= min && v <= max {
                opcodes.insert((opcode.count, opcode.number), opcode);
            }
        };

        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Zero, 0x00, "rtrue"          ).returns());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Zero, 0x01, "rfalse"         ).returns());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Zero, 0x02, "print"          ).string());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Zero, 0x03, "print_ret"      ).returns().string());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Zero, 0x04, "nop"            ));
        op(ZVersion::V1, ZVersion::V3, Opcode::new(Count::Zero, 0x05, "save"           ).branch());
        op(ZVersion::V4, ZVersion::V4, Opcode::new(Count::Zero, 0x05, "save"           ).store());
        op(ZVersion::V1, ZVersion::V3, Opcode::new(Count::Zero, 0x06, "restore"        ).branch());
        op(ZVersion::V4, ZVersion::V4, Opcode::new(Count::Zero, 0x06, "restore"        ).store());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Zero, 0x07, "restart"        ).returns());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Zero, 0x08, "ret_popped"     ).returns());
        op(ZVersion::V1, ZVersion::V4, Opcode::new(Count::Zero, 0x09, "pop"            ));
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Zero, 0x09, "catch"          ).store());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Zero, 0x0a, "quit"           ).returns());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Zero, 0x0b, "new_line"       ));
        op(ZVersion::V3, ZVersion::V3, Opcode::new(Count::Zero, 0x0c, "show_status"    ));
        op(ZVersion::V3, ZVersion::V6, Opcode::new(Count::Zero, 0x0d, "verify"         ).branch());
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Zero, 0x0f, "piracy"         ).branch());

        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::One,  0x00, "jz"             ).branch());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::One,  0x01, "get_sibling"    ).store().branch());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::One,  0x02, "get_child"      ).store().branch());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::One,  0x03, "get_parent"     ).store());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::One,  0x04, "get_prop_len"   ).store());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::One,  0x05, "inc"            ).indirect());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::One,  0x06, "dec"            ).indirect());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::One,  0x07, "print_addr"     ).paddr());
        op(ZVersion::V4, ZVersion::V6, Opcode::new(Count::One,  0x08, "call_1s"        ).store().call(0));
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::One,  0x09, "remove_obj"     ));
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::One,  0x0a, "print_obj"      ));
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::One,  0x0b, "ret"            ).returns());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::One,  0x0c, "jump"           ).returns().jump());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::One,  0x0d, "print_paddr"    ).ppaddr());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::One,  0x0e, "load"           ).store().indirect());
        op(ZVersion::V1, ZVersion::V4, Opcode::new(Count::One,  0x0f, "not"            ).store());
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::One,  0x0f, "call_1n"        ).call(0));

        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x01, "je"             ).branch());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x02, "jl"             ).branch());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x03, "jg"             ).branch());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x04, "dec_chk"        ).branch().indirect());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x05, "inc_chk"        ).branch().indirect());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x06, "jin"            ).branch());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x07, "test"           ).branch());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x08, "or"             ).store());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x09, "and"            ).store());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x0a, "test_attr"      ).branch());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x0b, "set_attr"       ));
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x0c, "clear_attr"     ));
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x0d, "store"          ).indirect());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x0e, "insert_obj"     ));
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x0f, "loadw"          ).store());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x10, "loadb"          ).store());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x11, "get_prop"       ).store());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x12, "get_prop_addr"  ).store());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x13, "get_next_prop"  ).store());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x14, "add"            ).store());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x15, "sub"            ).store());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x16, "mul"            ).store());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x17, "div"            ).store());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Two,  0x18, "mod"            ).store());
        op(ZVersion::V4, ZVersion::V6, Opcode::new(Count::Two,  0x19, "call_2s"        ).store().call(0));
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Two,  0x1a, "call_2n"        ).call(0));
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Two,  0x1b, "set_colour"     ));
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Two,  0x1c, "throw"          ));

        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Var,  0x00, "call_vs"        ).store().call(0));
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Var,  0x01, "storew"         ));
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Var,  0x02, "storeb"         ));
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Var,  0x03, "put_prop"       ));
        op(ZVersion::V1, ZVersion::V3, Opcode::new(Count::Var,  0x04, "read"           ));
        op(ZVersion::V4, ZVersion::V4, Opcode::new(Count::Var,  0x04, "read"           ).call(3));
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Var,  0x04, "read"           ).store().call(3));
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Var,  0x05, "print_char"     ).printchar());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Var,  0x06, "print_num"      ));
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Var,  0x07, "random"         ).store());
        op(ZVersion::V1, ZVersion::V6, Opcode::new(Count::Var,  0x08, "push"           ));
        op(ZVersion::V1, ZVersion::V5, Opcode::new(Count::Var,  0x09, "pull"           ).indirect());
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Var,  0x09, "pull"           ).store());
        op(ZVersion::V3, ZVersion::V6, Opcode::new(Count::Var,  0x0a, "split_window"   ));
        op(ZVersion::V3, ZVersion::V6, Opcode::new(Count::Var,  0x0b, "set_window"     ));
        op(ZVersion::V4, ZVersion::V6, Opcode::new(Count::Var,  0x0c, "call_vs2"       ).store().call(0));
        op(ZVersion::V4, ZVersion::V6, Opcode::new(Count::Var,  0x0d, "erase_window"   ));
        op(ZVersion::V4, ZVersion::V6, Opcode::new(Count::Var,  0x0e, "erase_line"     ));
        op(ZVersion::V4, ZVersion::V6, Opcode::new(Count::Var,  0x0f, "set_cursor"     ));
        op(ZVersion::V4, ZVersion::V6, Opcode::new(Count::Var,  0x10, "get_cursor"     ));
        op(ZVersion::V4, ZVersion::V6, Opcode::new(Count::Var,  0x11, "set_text_style" ));
        op(ZVersion::V4, ZVersion::V6, Opcode::new(Count::Var,  0x12, "buffer_mode"    ));
        op(ZVersion::V3, ZVersion::V6, Opcode::new(Count::Var,  0x13, "output_stream"  ));
        op(ZVersion::V3, ZVersion::V6, Opcode::new(Count::Var,  0x14, "input_stream"   ));
        op(ZVersion::V3, ZVersion::V4, Opcode::new(Count::Var,  0x15, "sound_effect"   ));
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Var,  0x15, "sound_effect"   ).call(3));
        op(ZVersion::V4, ZVersion::V6, Opcode::new(Count::Var,  0x16, "read_char"      ).store().call(2));
        op(ZVersion::V4, ZVersion::V6, Opcode::new(Count::Var,  0x17, "scan_table"     ).store().branch());
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Var,  0x18, "not"            ).store());
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Var,  0x19, "call_vn"        ).call(0));
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Var,  0x1a, "call_vn2"       ).call(0));
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Var,  0x1b, "tokenise"       ));
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Var,  0x1c, "encode_text"    ));
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Var,  0x1d, "copy_table"     ));
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Var,  0x1e, "print_table"    ));
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Var,  0x1f, "check_arg_count").branch());

        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Ext,  0x00, "save"           ).store());
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Ext,  0x01, "restore"        ).store());
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Ext,  0x02, "log_shift"      ).store());
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Ext,  0x03, "art_shift"      ).store());
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Ext,  0x04, "set_font"       ).store());
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Ext,  0x05, "draw_picture"   ));
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Ext,  0x06, "picture_data"   ).branch());
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Ext,  0x07, "erase_picture"  ));
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Ext,  0x08, "set_margins"    ));
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Ext,  0x09, "save_undo"      ).store());
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Ext,  0x0a, "restore_undo"   ).store());
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Ext,  0x0b, "print_unicode"  ).punicode());
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Ext,  0x0c, "check_unicode"  ).store());
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Ext,  0x0d, "set_true_colour"));
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Ext,  0x10, "move_window"    ));
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Ext,  0x11, "window_size"    ));
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Ext,  0x12, "window_style"   ));
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Ext,  0x13, "get_wind_prop"  ).store());
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Ext,  0x14, "scroll_window"  ));
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Ext,  0x15, "pop_stack"      ));
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Ext,  0x16, "read_mouse"     ));
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Ext,  0x17, "mouse_window"   ));
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Ext,  0x18, "push_stack"     ).branch());
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Ext,  0x19, "put_wind_prop"  ));
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Ext,  0x1a, "print_form"     ));
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Ext,  0x1b, "make_menu"      ).branch());
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Ext,  0x1c, "picture_table"  ));
        op(ZVersion::V6, ZVersion::V6, Opcode::new(Count::Ext,  0x1d, "buffer_screen"  ));

        // Zoom extensions.
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Ext,  0x80, "start_timer"    ));
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Ext,  0x81, "stop_timer"     ));
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Ext,  0x82, "read_timer"     ).store());
        op(ZVersion::V5, ZVersion::V6, Opcode::new(Count::Ext,  0x83, "print_timer"    ));

        // This is invalid, but the Solid Gold Wishbringer calls
        // @show_status. If it were made illegal, disassembly would fail
        // upon encountering it. The Standard recommends treating it as
        // @nop. Decode it as “wb_nop” here to differentiate from a real
        // @nop.
        // The invalid call isn’t reachable from the initial PC, but can
        // be seen by disassembling at address 0x1f8a4, and looking at
        // the instruction at address 0x1f910.
        op(ZVersion::V5, ZVersion::V5, Opcode::new(Count::Zero, 0x0c, "wb_nop"         ));

        opcodes
    }

    pub(crate) fn start(&mut self, addresses: Option<Vec<usize>>) -> Result<DisassembledStory, Error> {
        let mut seen_addresses = HashSet::new();
        let mut instructions = Vec::new();
        let mut routines = HashMap::new();

        let main_routine = if self.zversion == ZVersion::V6 {
            let packed = self.source.start();
            self.handle_routine(packed, &mut routines)?.ok_or(Error::InvalidStart(packed))?
        } else {
            self.source.start().into()
        };

        let addresses = if let Some(start_offset) = self.options.start_offset {
            vec![start_offset]
        } else {
            addresses
                .unwrap_or_else(|| vec![0])
                .iter()
                .map(|addr| if *addr == 0 {
                    Ok(main_routine)
                } else {
                    let packed = self.pack(*addr, false)
                        .map_err(|_| Error::PackedAddressTooLarge(*addr))?;
                    self.handle_routine(packed, &mut routines)?
                        .ok_or(Error::InvalidRoutine(*addr))
                })
                .collect::<Result<Vec<_>, _>>()?
        };

        for address in addresses {
            if self.options.verbose {
                println!("Beginning disassembly at {address:04x}");
                println!("-----------------------------");
            }

            self.pc = address;
            self.interpret(0, &mut seen_addresses, &mut instructions, &mut routines)?;

            if self.options.verbose {
                println!("\n{}", "-".repeat(50));
            }
        }

        instructions.sort_unstable_by(|l, r| l.addr.cmp(&r.addr));

        Ok(DisassembledStory {
            zversion: self.zversion,
            instructions,
            routines,
            main_routine,
        })
    }

    fn interpret(&mut self, indent: usize, seen_addresses: &mut HashSet<usize>, instructions: &mut Vec<Instruction>, routines: &mut HashMap<usize, Routine>) -> Result<(), Error> {
        macro_rules! iprint {
            ($($arg:tt)*) => {
                if self.options.verbose {
                    print!($($arg)*)
                }
            }
        }

        if self.pc >= self.source.memory().size() {
            iprint!("{}", " ".repeat(indent * 2));
            iprint!("{:04x}: outside of memory\n", self.pc);

            if self.options.ignore_errors {
                return Ok(());
            } else {
                return Err(Error::InvalidJump(self.pc));
            }
        }

        loop {
            if let Some(max_instructions) = self.options.max_instructions {
                if instructions.len() >= max_instructions {
                    iprint!("stopping: reached max instruction count ({max_instructions})\n");
                    return Ok(())
                }
            }

            if !seen_addresses.insert(self.pc) {
                return Ok(());
            }

            let orig_pc = self.pc;

            let opcode = match self.next_byte() {
                Ok(byte) => byte,
                Err(Error::EndOfFile { .. }) if self.source.eof_is_quit() => return Ok(()),
                Err(e) => return Err(e),
            };

            let mut args = Vec::new();
            let form;
            let count;
            let number;

            if opcode < 0x80 {
                if opcode & 0x40 == 0x40 {
                    args.push(Arg::Variable(self.next_byte()?));
                } else {
                    args.push(Arg::Constant(self.next_byte()?.into()));
                }

                if opcode & 0x20 == 0x20 {
                    args.push(Arg::Variable(self.next_byte()?));
                } else {
                    args.push(Arg::Constant(self.next_byte()?.into()));
                }

                form = Form::Long;
                count = Count::Two;
                number = opcode & 0x1f;
            } else if opcode < 0xb0 {
                if opcode < 0x90 {
                    args.push(Arg::Constant(self.next_word()?));
                } else if opcode < 0xa0 {
                    args.push(Arg::Constant(self.next_byte()?.into()));
                } else {
                    args.push(Arg::Variable(self.next_byte()?));
                }

                form = Form::Short;
                count = Count::One;
                number = opcode & 0x0f;
            } else if opcode < 0xc0 {
                if self.zversion >= ZVersion::V5 && opcode == 0xbe {
                    form = Form::Extended;
                    count = Count::Ext;
                    number = self.next_byte()?;

                    let byte = self.next_byte()?;
                    self.decode_var(byte, &mut args)?;
                } else {
                    form = Form::Short;
                    count = Count::Zero;
                    number = opcode & 0x0f;
                }
            } else if opcode < 0xe0 {
                let byte = self.next_byte()?;
                self.decode_var(byte, &mut args)?;

                form = Form::Variable;
                count = Count::Two;
                number = opcode & 0x1f;
            } else if opcode == 0xec || opcode == 0xfa {
                let types1 = self.next_byte()?;
                let types2 = self.next_byte()?;

                self.decode_var(types1, &mut args)?;
                self.decode_var(types2, &mut args)?;

                form = Form::Variable;
                count = Count::Var;
                number = opcode & 0x1f;
            } else {
                let byte = self.next_byte()?;

                self.decode_var(byte, &mut args)?;

                form = Form::Variable;
                count = Count::Var;
                number = opcode & 0x1f;
            }

            iprint!("{}{:04x}: ", " ".repeat(indent * 2), orig_pc);

            let opcode = match self.opcodes.get(&(count, number.into())) {
                Some(opcode) => opcode.clone(),
                None => {
                    iprint!("unknown opcode {}: {}\n", count, number);

                    if self.options.ignore_errors {
                        return Ok(());
                    } else if self.options.verbose {
                        return Err(Error::CannotContinue);
                    } else {
                        return Err(Error::UnknownOpcode { count, number, pc: orig_pc });
                    }
                }
            };

            let mut instruction = Instruction::new(&opcode, form);

            instruction.full_opcode = self.source.memory().slice(orig_pc, self.pc - orig_pc)?.to_vec();

            iprint!("{} ", opcode.name);

            for arg in &args {
                iprint!("{} ", arg);
            }

            if opcode.store {
                let b = self.next_byte()?;
                iprint!("-> {}", Arg::Store(b));
                instruction.storeto = Some(b);
            }

            let mut newpc = None;

            if opcode.branch {
                let branch = self.next_byte()?;

                let mut offset = u16::from(branch & 0x3f);

                let size = if branch & 0x40 == 0 {
                    let second = self.next_byte()?;

                    offset = (offset << 8) | u16::from(second);

                    if offset & 0x2000 == 0x2000 {
                        offset |= 0xc000;
                    }

                    BranchSize::Long(branch, second)
                } else {
                    BranchSize::Short(branch)
                };

                let invert = branch & 0x80 != 0x80;

                let target = match offset {
                    0 => BranchTarget::ReturnFalse,
                    1 => BranchTarget::ReturnTrue,
                    _ => {
                        let offset = offset.as_signed();
                        let address = ((self.pc as isize) + offset - 2) as usize;
                        newpc = Some(address);
                        BranchTarget::Where { offset, address }
                    }
                };

                let branch = Branch {
                    size,
                    invert,
                    target,
                };

                iprint!("{}", branch);

                instruction.branch = Some(branch);
            }

            if opcode.string {
                let addr = self.pc;
                let (n, string, parts) = self.decode_string(addr)?;

                self.pc += n;
                instruction.string = Some(ZString { string, parts });
            }

            if opcode.printchar {
                if let Some(&Arg::Constant(arg)) = args.first() {
                    instruction.character = Some(self.zscii_to_unicode(arg, true));
                }
            }

            if opcode.punicode {
                if let Some(&Arg::Constant(arg)) = args.first() {
                    instruction.punicode = Some(char::from_u32(arg.into()).unwrap_or('�'));
                }
            }

            if opcode.paddr && self.source.can_branch() {
                if let Some(&Arg::Constant(addr)) = args.first() {
                    let (_, string, parts) = self.decode_string(addr.into())?;
                    instruction.string = Some(ZString { string, parts });
                }
            }

            if opcode.ppaddr && self.source.can_branch() {
                if let Some(&Arg::Constant(addr)) = args.first() {
                    let addr = self.unpack(addr, true);
                    let (_, string, parts) = self.decode_string(addr)?;
                    instruction.string = Some(ZString { string, parts });
                }
            }

            if let Some(arg) = opcode.call {
                if let Some(&Arg::Constant(packed)) = args.get(arg) {
                    let addr = self.unpack(packed, false);
                    args[arg] = Arg::Call(packed, addr);
                    instruction.call = Some(Call { packed, addr });
                    newpc = self.handle_routine(packed, routines)?;
                }
            }

            if opcode.jump {
                if let Some(&Arg::Constant(offset)) = args.first() {
                    let to = ((self.pc as isize) + offset.as_signed() - 2) as usize;
                    args[0] = Arg::Jump(offset, to);
                    instruction.jump = Some(Jump { to });
                    newpc = Some(to);
                }
            }

            instruction.addr = orig_pc;
            instruction.nextaddr = self.pc;
            instruction.full_instruction = self.source.memory().slice(orig_pc, self.pc - orig_pc)?.to_vec();
            instruction.args = args;

            instructions.push(instruction);

            iprint!("\n");

            if let (Some(pc), true) = (newpc, self.source.can_branch()) {
                let saved = self.pc;
                self.pc = pc;
                self.interpret(indent + 1, seen_addresses, instructions, routines)?;
                self.pc = saved;
            }

            if opcode.returns {
                return Ok(());
            }
        }
    }

    fn decode_var(&mut self, types: u8, args: &mut Vec<Arg>) -> Result<(), Error> {
        for i in &[6, 4, 2, 0] {
            match OperandType::try_from((types >> *i) & 0x03)? {
                OperandType::Large => args.push(Arg::Constant(self.next_word()?)),
                OperandType::Small => args.push(Arg::Constant(self.next_byte()?.into())),
                OperandType::Variable => args.push(Arg::Variable(self.next_byte()?)),
                OperandType::Omitted => return Ok(()),
            }
        }

        Ok(())
    }

    fn handle_routine(&mut self, packed: u16, routines: &mut HashMap<usize, Routine>) -> Result<Option<usize>, Error> {
        if packed == 0 {
            return Ok(None);
        }

        let addr = self.unpack(packed, false);
        let nlocals = if self.source.can_branch() {
            usize::from(self.source.memory().byte(addr)?)
        } else {
            0
        };

        let (locals, newaddr) = if self.zversion <= ZVersion::V4 {
            (self.source.memory().slice_words(addr + 1, nlocals)?, addr + 1 + (nlocals * 2))
        } else {
            (vec![0; nlocals], addr + 1)
        };

        let routine = Routine {
            addr,
            locals,
        };

        routines.insert(newaddr, routine);

        Ok(Some(newaddr))
    }

    fn pack(&self, addr: usize, string: bool) -> Result<u16, std::num::TryFromIntError> {
        let packed = match self.zversion {
            ZVersion::V1 | ZVersion::V2 | ZVersion::V3 => addr / 2,
            ZVersion::V4 | ZVersion::V5 => addr / 4,
            ZVersion::V6 | ZVersion::V7 => addr - (if string { self.source.s_o() } else { self.source.r_o() }) / 4,
            ZVersion::V8 => addr / 8,
        };

        u16::try_from(packed)
    }

    fn unpack(&self, packed: u16, string: bool) -> usize {
        let packed = usize::from(packed);
        match self.zversion {
            ZVersion::V1 | ZVersion::V2 | ZVersion::V3 => packed * 2,
            ZVersion::V4 | ZVersion::V5 => packed * 4,
            ZVersion::V6 | ZVersion::V7 => (packed * 4) + (if string { self.source.s_o() } else { self.source.r_o() }),
            ZVersion::V8 => packed * 8,
        }
    }

    fn decode_string(&mut self, addr: usize) -> Result<DecodedString, Error> {
        self.decode_string_helper(addr, false)
    }

    fn decode_string_helper(&mut self, addr: usize, in_abbr: bool) -> Result<DecodedString, Error> {
        let mut abbrev = None;
        let mut current_alphabet = 0;
        let mut shift = 0;
        let mut current = addr;
        let mut s = String::new();
        let mut chunks = Vec::new();

        #[derive(Copy, Clone)]
        enum TenBit {
            Start,
            First(u16),
        }

        let mut tenbit = None;

        loop {
            let word = self.source.memory().word(current)?;
            current += 2;

            let mut handle_character = |c| -> Result<StringPart, Error> {
                if let Some(tb) = tenbit {
                    match tb {
                        TenBit::Start => {
                            tenbit = Some(TenBit::First(c));

                            Ok(StringPart::ZSCIITop)
                        }
                        TenBit::First(lastc) => {
                            let character = (lastc << 5) | c;

                            s.push_str(&self.zscii_to_unicode(character, false));

                            tenbit = None;

                            Ok(StringPart::ZSCIIBottom(character, self.zscii_to_unicode(character, true)))
                        }
                    }
                } else if let Some(offset) = abbrev {
                    if let Some(abbr_table) = self.source.abbr_table() {
                        let new_addr = usize::from(self.source.memory().word(abbr_table + 64 * (usize::from(offset) - 1) + 2 * usize::from(c))?);

                        let (_, abbr, _) = self.decode_string_helper(new_addr * 2, true)?;

                        s.push_str(&abbr);

                        abbrev = None;

                        Ok(StringPart::Abbrev(c, abbr))
                    } else {
                        s.push_str("[abbr]");
                        abbrev = None;
                        Ok(StringPart::AbbrevOmitted)
                    }
                } else {
                    match c {
                        0 => {
                            s.push(' ');
                            shift = 0;

                            Ok(StringPart::Space(self.zscii_to_unicode(ZSCII::Space as u16, true)))
                        }
                        1 if self.zversion == ZVersion::V1 => {
                            s.push('\n');
                            shift = 0;

                            Ok(StringPart::Newline(self.zscii_to_unicode(ZSCII::Newline as u16, true)))
                        }
                        2 | 3 if self.zversion <= ZVersion::V2 => {
                            shift = c - 1;

                            Ok(StringPart::Shift(shift))
                        }
                        1 | 2 | 3 => {
                            if in_abbr {
                                if self.options.ignore_errors {
                                    Ok(StringPart::Character("[recursive abbreviation]".into()))
                                } else {
                                    Err(Error::RecursiveAbbreviation)
                                }
                            } else {
                                abbrev = Some(c);
                                shift = 0;

                                Ok(StringPart::AbbrevStart(c))
                            }
                        }
                        4 | 5 if self.zversion <= ZVersion::V2 => {
                            let old_alphabet = current_alphabet;
                            current_alphabet = (current_alphabet + c - 3) % 3;
                            shift = 0;

                            Ok(StringPart::ShiftLock(old_alphabet, current_alphabet))
                        }
                        4 | 5 => {
                            shift = c - 3;

                            Ok(StringPart::Shift(shift))
                        }
                        6 if (current_alphabet + shift) % 3 == 2 => {
                            tenbit = Some(TenBit::Start);
                            shift = 0;

                            Ok(StringPart::ZSCIIStart)
                        }
                        c => {
                            shift = (current_alphabet + shift) % 3;

                            let decoded = self.source.atable()[(26 * usize::from(shift)) + usize::from(c - 6)];

                            s.push_str(&self.zscii_to_unicode(decoded.into(), false));

                            shift = 0;

                            Ok(StringPart::Character(self.zscii_to_unicode(decoded.into(), true)))
                        }
                    }
                }
            };

            let parts = [
                handle_character((word >> 10) & 0x1f)?,
                handle_character((word >>  5) & 0x1f)?,
                handle_character((word      ) & 0x1f)?,
            ];

            chunks.push((word, parts));

            if word & 0x8000 == 0x8000 {
                break;
            }
        }

        if self.options.newline_char {
            s = s.replace('\n', "^");
        }

        Ok((current - addr, s, chunks))
    }

    fn zscii_to_unicode(&self, zscii: u16, longform: bool) -> String {
        match zscii {
            0 => if longform { "null" } else { "␀" }.to_string(),
            9 if self.zversion == ZVersion::V6 => if longform { "tab" } else { "␉" }.to_string(),
            11 if self.zversion == ZVersion::V6 => if longform { "sentence space" } else { "␠" }.to_string(),
            13 => if longform { "newline" } else { "\n" }.to_string(),
            32 => if longform { "space" } else { " " }.to_string(),
            33..=126 => format!("{}", zscii as u8 as char),
            155..=251 => {
                char::from_u32(self.source.unicode_table()[usize::from(zscii) - 155].into())
                    .map(String::from)
                    .unwrap_or_else(|| if longform {
                        format!("invalid unicode zscii: {zscii}")
                    } else {
                        "�".to_string()
                    })
            }
            _ => if longform { format!("out of bounds zscii character: {zscii}") } else { "�".to_string() },
        }
    }

    fn next_byte(&mut self) -> Result<u8, Error> {
        let byte = self.source.memory().byte(self.pc);
        self.pc += 1;
        byte
    }

    fn next_word(&mut self) -> Result<u16, Error> {
        let word = self.source.memory().word(self.pc);
        self.pc += 2;
        word
    }
}
