use super::{Arg, Count, Opcode, Variable, ZVersion};

lalrpop_mod!(z1, "/asm/parser/z1.rs");
lalrpop_mod!(z2, "/asm/parser/z2.rs");
lalrpop_mod!(z3, "/asm/parser/z3.rs");
lalrpop_mod!(z4, "/asm/parser/z4.rs");
lalrpop_mod!(z5, "/asm/parser/z5.rs");
lalrpop_mod!(z6, "/asm/parser/z6.rs");

#[derive(Debug, Copy, Clone)]
pub(crate) enum StatusType {
    Score,
    Time,
}

#[derive(Debug, Clone)]
pub(crate) enum Branch {
    Labeled { label: String, invert: bool, small: bool },
    ReturnTrue { invert: bool },
    ReturnFalse { invert: bool },
}

#[derive(Debug)]
pub(crate) struct StartV6Args {
    pub(crate) label: String,
    pub(crate) nlocals: usize,
}

#[derive(Debug)]
pub(crate) enum Directive {
    Instruction(Opcode),
    Start,
    StartV6(StartV6Args),
    DefaultAlign,
    Align { alignment: usize },
    Routine { label: String, nlocals: usize, values: Vec<usize> },
    Status(StatusType),
    Seek { bytes: usize },
    SeekAbs { offset: usize },
    SeekNop { bytes: usize },
    SeekNopAbs { offset: usize },
    Label(String),
    AlignedLabel(String),
    String(String),
    Byte(Vec<usize>),
    SetGlobal(Variable, usize),
    UnicodeTable(Vec<char>),
}

#[derive(Debug)]
pub(crate) enum Error {
    InvalidToken(usize),
    UnrecognizedToken((usize, usize), Vec<String>),
    UnexpectedEOF(usize),
    ExtraToken(usize, usize),
    User(String),
}

macro_rules! convert {
    ($e:expr) => {
        match $e {
            Ok(directive) => Ok(directive),
            Err(e) => Err(match e {
                lalrpop_util::ParseError::InvalidToken { location } => Error::InvalidToken(location),
                lalrpop_util::ParseError::UnrecognizedToken { token: (lo, _, hi), expected } => Error::UnrecognizedToken((lo, hi), expected),
                lalrpop_util::ParseError::UnrecognizedEof { location, .. } => Error::UnexpectedEOF(location),
                lalrpop_util::ParseError::ExtraToken { token: (lo, _, hi) } => Error::ExtraToken(lo, hi),
                lalrpop_util::ParseError::User { error } => Error::User(error.to_string()),
            })
        }
    }
}

pub(crate) fn parse_directive(version: ZVersion, line: &str) -> Result<Directive, Error> {
    match version {
        ZVersion::V1 => convert!(z1::DirectiveParser::new().parse(line)),
        ZVersion::V2 => convert!(z2::DirectiveParser::new().parse(line)),
        ZVersion::V3 => convert!(z3::DirectiveParser::new().parse(line)),
        ZVersion::V4 => convert!(z4::DirectiveParser::new().parse(line)),
        ZVersion::V5 | ZVersion::V7 | ZVersion::V8 => convert!(z5::DirectiveParser::new().parse(line)),
        ZVersion::V6 => convert!(z6::DirectiveParser::new().parse(line)),
    }
}
