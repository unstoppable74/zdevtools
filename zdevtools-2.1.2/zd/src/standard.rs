use super::dis;

use dis::{Arg, DisassembledStory};

pub(crate) struct Printer<'a> {
    disassembled: &'a DisassembledStory,
    dumphex: bool,
    dumpstring: bool,
    print_width: usize,
}

impl<'a> Printer<'a> {
    pub(crate) fn new(disassembled: &'a DisassembledStory, dumphex: bool, dumpstring: bool, print_width: usize) -> Self {
        Printer {
            disassembled,
            dumphex,
            dumpstring,
            print_width,
        }
    }

    pub(crate) fn print(&self) {
        let mut lastaddr = 0;

        for instruction in &self.disassembled.instructions {
            if lastaddr != instruction.addr {
                println!();
            }

            if instruction.addr == self.disassembled.main_routine {
                if lastaddr == instruction.addr {
                    println!();
                }

                println!("MAIN ROUTINE {:x}", instruction.addr);
            } else if let Some(routine) = self.disassembled.routines.get(&instruction.addr) {
                print!("ROUTINE {:x} ({} local{}", routine.addr, routine.locals.len(), if routine.locals.len() == 1 { "" } else { "s" });

                if !routine.locals.is_empty() {
                    let args = routine.locals
                        .iter()
                        .map(|arg| arg.to_string())
                        .collect::<Vec<_>>()
                        .join(", ");
                    print!(": {args}");
                }

                println!(")");
            }

            lastaddr = instruction.nextaddr;

            print!("{:6x}: ", instruction.addr);

            fn roundup(v: usize, multiple: usize) -> usize {
                ((v + multiple - 1) / multiple) * multiple
            }

            if self.dumphex {
                if !self.dumpstring && instruction.opcode.string {
                    print!("{:02x} ...{}", instruction.full_opcode[0], " ".repeat((self.print_width * 3) - 6));
                } else {
                    for (i, c) in instruction.full_instruction.iter().enumerate() {
                        if i > 0 && i % self.print_width == 0 {
                            print!("\n{}", " ".repeat(8));
                        }

                        print!("{c:02x} ");
                    }

                    let spaces = roundup(instruction.full_instruction.len(), self.print_width) - instruction.full_instruction.len();
                    print!("{}", "   ".repeat(spaces));
                }

                print!(" ");
            }

            print!("{:16} ", instruction.opcode.name);

            for (i, arg) in instruction.args.iter().enumerate() {
                match (i, instruction.opcode.indirect, arg) {
                    (0, true, &Arg::Variable(_)) => print!("[{arg}] "),
                    (0, true, &Arg::Constant(0)) => print!("SP "),
                    (0, true, &Arg::Constant(val)) if val < 0x100 => print!("{} ", Arg::Variable(val as u8)),
                    (0, true, &Arg::Constant(val)) => print!("[invalid variable: #{val:x}] "),
                    _ => print!("{arg} "),
                }
            }

            if let Some(storeto) = instruction.storeto {
                print!("-> {} ", Arg::Store(storeto));
            }

            if let Some(branch) = instruction.branch {
                print!("{branch}");
            }

            if let Some(ref string) = instruction.string {
                if instruction.opcode.paddr || instruction.opcode.ppaddr {
                    let repeat = 25 + if self.dumphex { self.print_width } else { 0 };
                    println!();
                    print!("{}", " ".repeat(repeat));
                }

                print!("{} ", string.string);
            }

            if let Some(ref character) = instruction.character {
                print!("({character}) ");
            }

            if let Some(character) = instruction.punicode {
                print!("({character}) ");
            }

            println!();
        }
    }
}
