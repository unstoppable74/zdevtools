use std::convert::{TryFrom, TryInto};
use std::fs;
use std::io::{self, Read};
use std::path;

use crate::dis;

pub(crate) struct BytesSource {
    memory: dis::Memory,
    zversion: dis::ZVersion,
}

impl BytesSource {
    pub(crate) fn new(bytes: &[u8], zversion: dis::ZVersion) -> Self {
        BytesSource {
            memory: dis::Memory::new(bytes.to_vec()),
            zversion,
        }
    }
}

impl dis::DisassemblySource for BytesSource {
    fn zversion(&self) -> dis::ZVersion {
        self.zversion
    }

    fn can_branch(&self) -> bool {
        false
    }

    fn start(&self) -> u16 {
        0
    }

    fn memory(&self) -> &dis::Memory {
        &self.memory
    }

    fn s_o(&self) -> usize {
        match self.zversion {
            dis::ZVersion::V6 | dis::ZVersion::V7 => panic!("can't use offsets in version {}", self.zversion),
            _ => 0,
        }
    }

    fn r_o(&self) -> usize {
        self.s_o()
    }

    fn abbr_table(&self) -> Option<usize> {
        None
    }

    fn eof_is_quit(&self) -> bool {
        true
    }
}

pub(crate) struct StorySource {
    memory: dis::Memory,
    zversion: dis::ZVersion,
    start: u16,
    s_o: usize,
    r_o: usize,

    abbr_table: usize,

    atable: dis::AlphabetTable,
    unicode_table: dis::UnicodeTable,
}

impl dis::DisassemblySource for StorySource {
    fn zversion(&self) -> dis::ZVersion {
        self.zversion
    }

    fn can_branch(&self) -> bool {
        true
    }

    fn start(&self) -> u16 {
        self.start
    }

    fn memory(&self) -> &dis::Memory {
        &self.memory
    }

    fn s_o(&self) -> usize {
        self.s_o
    }

    fn r_o(&self) -> usize {
        self.r_o
    }

    fn abbr_table(&self) -> Option<usize> {
        Some(self.abbr_table)
    }

    fn atable(&self) -> dis::AlphabetTable {
        self.atable
    }

    fn unicode_table(&self) -> dis::UnicodeTable {
        self.unicode_table
    }

    fn eof_is_quit(&self) -> bool {
        false
    }
}

#[derive(thiserror::Error, Debug)]
pub(crate) enum Error {
    #[error(transparent)]
    IO(#[from] io::Error),
    #[error(transparent)]
    Disassembly(#[from] dis::Error),
    #[error(transparent)]
    ZMachineVersrion(#[from] dis::ConvertError),
    #[error("invalid alphabet table")]
    InvalidAlphabetTable,
}

impl StorySource {
    pub(crate) fn new<P>(path: P) -> Result<Self, Error>
    where
        P: AsRef<path::Path>,
    {
        let mut mem = Vec::new();
        let mut f = fs::File::open(path)?;

        f.read_to_end(&mut mem)?;

        let memory = dis::Memory::new(mem);

        let zversion = dis::ZVersion::try_from(memory.byte(0)?)?;
        let start = memory.word(0x06)?;
        let abbr_table = usize::from(memory.word(0x18)?);

        let mut unicode_table = dis::DEFAULT_UNICODE_TABLE;

        let (r_o, s_o) = if zversion == dis::ZVersion::V6 || zversion == dis::ZVersion::V7 {
            ((usize::from(memory.word(0x28)?) * 8),
             (usize::from(memory.word(0x2a)?) * 8))
        } else {
            (0, 0)
        };

        let atable = if zversion == dis::ZVersion::V1 {
            dis::DEFAULT_ATABLE_V1
        } else if zversion >= dis::ZVersion::V5 && memory.word(0x34)? != 0 {
            let mut a = memory.slice(usize::from(memory.word(0x34)?), 26 * 3)?.to_vec();

            a[52] = 0x00;
            a[53] = 0x0d;

            a.try_into().map_err(|_| Error::InvalidAlphabetTable)?
        } else {
            dis::DEFAULT_ATABLE_V2
        };

        if zversion >= dis::ZVersion::V5 {
            let etable = usize::from(memory.word(0x36)?);

            if etable != 0 {
                let nentries = memory.word(etable)?;

                if nentries >= 3 {
                    let utable = usize::from(memory.word(etable + (2 * 3))?);

                    if utable != 0 {
                        let unicode_entries = usize::from(memory.byte(utable)?);

                        if unicode_entries > unicode_table.len() {
                            println!("corrupted story: unicode table out of range");
                        } else {
                            unicode_table[..unicode_entries].copy_from_slice(&memory.slice_words(utable + 1, unicode_entries)?);
                        }
                    }
                }
            }
        }

        Ok(StorySource {
            memory,
            zversion,
            start,
            s_o,
            r_o,

            abbr_table,

            atable,
            unicode_table,
        })
    }
}
