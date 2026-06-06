use std::convert::TryFrom;

use super::dis;

use dis::{Arg, AsSigned, BranchSize, BranchTarget, Count, DisassembledStory, Form, OperandType, ZVersion};

#[derive(thiserror::Error, Debug)]
pub(crate) enum Error {
    #[error(transparent)]
    Disassembly(#[from] dis::Error),
}

pub(crate) struct VisualPrinter<'a> {
    disassembled: &'a DisassembledStory,
}

impl<'a> VisualPrinter<'a> {
    pub(crate) fn new(disassembled: &'a DisassembledStory) -> Self {
        VisualPrinter {
            disassembled,
        }
    }

    pub(crate) fn print(&self) -> Result<(), Error> {
        for instruction in &self.disassembled.instructions {
            let mut full_opcode = instruction.full_opcode.clone();

            let b = full_opcode.remove(0);

            if instruction.addr == self.disassembled.main_routine {
                self.print_box(70, &format!("{:x}: {} - {} (main routine {:x})", instruction.addr, instruction.opcode.name, instruction.opcode.count, instruction.addr));
            } else if let Some(routine) = self.disassembled.routines.get(&instruction.addr) {
                self.print_box(70, &format!("{:x}: {} - {} (routine {:x})", instruction.addr, instruction.opcode.name, instruction.opcode.count, routine.addr));
            } else {
                self.print_box(70, &format!("{:x}: {} - {}", instruction.addr, instruction.opcode.name, instruction.opcode.count));
            }

            if instruction.opcode.count == Count::Ext {
                println!("       ┌─ Extended form");
                println!("       │               ┌─ @{}", instruction.opcode.name);
                println!("┌──────┴──────┐ ┌──────┴──────┐");

                self.print_spaced(b, false);
                self.print_spaced(full_opcode.remove(0), true);

                println!();

                self.print_varext_operands(&mut full_opcode, false)?;
            } else if instruction.form == Form::Variable {
                let is_dvar = self.disassembled.zversion >= ZVersion::V4 && (b == 0xec || b == 0xfa);

                println!(" ┌─ Variable form");
                println!(" │  ┌─ {}", if b & 0x20 == 0x20 { "VAR" } else { "2OP" });
                println!(" │  │     ┌─ @{}", instruction.opcode.name);
                println!("┌┴┐ │ ┌───┴───┐");

                self.print_spaced(b, true);

                println!();
                self.print_varext_operands(&mut full_opcode, is_dvar)?;
            } else if instruction.form == Form::Short {
                let optype = OperandType::try_from((b & 0x30) >> 4)?;

                println!(" ┌─ Short form");
                println!(" │   ┌─ Operand type: {optype}");
                println!(" │   │     ┌─ @{}", instruction.opcode.name);
                println!("┌┴┐ ┌┴┐ ┌──┴──┐");

                self.print_spaced(b, true);

                if optype != OperandType::Omitted {
                    print!("\nOperand: ");
                    self.print_operand(optype, &mut full_opcode);
                }
            } else {
                let optypes = &["Small constant", "Variable"];
                let first = (b as usize & 0x40) >> 6;
                let second = (b as usize & 0x20) >> 5;

                println!("┌─ Long form");
                println!("│ ┌─ First operand: {}", optypes[first]);
                println!("│ │ ┌─ Second operand: {}", optypes[second]);
                println!("│ │ │     ┌─ @{}", instruction.opcode.name);
                println!("│ │ │ ┌───┴───┐");

                self.print_spaced(b, true);

                println!();

                print!("Operand 1: ");
                self.print_operand(if first == 0 { OperandType::Small } else { OperandType::Variable }, &mut full_opcode);
                print!("Operand 2: ");
                self.print_operand(if second == 0 { OperandType::Small } else { OperandType::Variable }, &mut full_opcode);
            }

            if let Some(call) = instruction.call {
                if call.packed != 0 {
                    println!("\nCall: {:x}", call.addr);
                }
            }

            if let Some(b) = instruction.storeto {
                println!("\nStore: {:08b} ({}) -> {}", b, b, Arg::Store(b));
            }

            if let Some(branch) = instruction.branch {
                println!();

                let branch_if = if branch.invert {
                    "false"
                } else {
                    "true"
                };

                let string = match branch.target {
                    BranchTarget::ReturnFalse => "Return false".to_string(),
                    BranchTarget::ReturnTrue => "Return true".to_string(),
                    BranchTarget::Where { offset, address } => format!("{offset:+} ({address:x})"),
                };

                match branch.size {
                    BranchSize::Long(first, second) => {
                        println!("┌─ Branch if {branch_if}");
                        println!("│ ┌─ 14-bit branch");
                        println!("│ │              ┌─ {string}");
                        println!("│ │ ┌────────────┴────────────┐");

                        self.print_spaced(first, false);
                        self.print_spaced(second, true);
                    }
                    BranchSize::Short(byte) => {
                        println!("┌─ Branch if {branch_if}");
                        println!("│ ┌─ 6-bit branch");
                        println!("│ │      ┌─ {string}");
                        println!("│ │ ┌────┴────┐");

                        self.print_spaced(byte, true);
                    }
                }
            }

            if let Some(ref string) = instruction.string {
                for &(word, ref parts) in &string.parts {
                    println!();
                    println!("┌─ {}", if word & 0x8000 == 0x8000 { "Last word" } else { "Words follow" });
                    println!("│     ┌─ {}", parts[0]);
                    println!("│     │         ┌─ {}", parts[1]);
                    println!("│     │         │         ┌─ {}", parts[2]);
                    println!("│ ┌───┴───┐ ┌───┴───┐ ┌───┴───┐");

                    self.print_spaced((word >> 8) as u8, false);
                    self.print_spaced((word & 0xff) as u8, true);
                }

                println!("\nFull decoded text: {}", string.string);
            }

            if let Some(jump) = instruction.jump {
                println!("\nJump to {:x}", jump.to);
            }

            if let Some(ref character) = instruction.character {
                println!("\nCharacter: {character}");
            }

            if let Some(character) = instruction.punicode {
                println!("\nUnicode character: {character}");
            }
        }

        Ok(())
    }

    fn print_box(&self, width: usize, msg: &str) {
        print!("┌");
        print!("{}", "─".repeat(width - 2));
        print!("┐\n│{msg}");
        print!("{}", " ".repeat(width - msg.len() - 2));
        print!("│\n└");
        print!("{}", "─".repeat(width - 2));
        println!("┘");
    }

    fn print_varext_operands(&self, bytes: &mut Vec<u8>, is_dvar: bool) -> Result<(), Error> {
        let mut ops = Vec::new();

        {
            let mut display_types = |byte: u8| -> Result<(), Error> {
                let a = OperandType::try_from((byte & 0xc0) >> 6)?;
                let b = OperandType::try_from((byte & 0x30) >> 4)?;
                let c = OperandType::try_from((byte & 0x0c) >> 2)?;
                let d = OperandType::try_from( byte & 0x03      )?;

                println!("Operand types:");
                println!(" ┌─ {a}");
                println!(" │   ┌─ {b}");
                println!(" │   │   ┌─ {c}");
                println!(" │   │   │   ┌─ {d}");
                println!("┌┴┐ ┌┴┐ ┌┴┐ ┌┴┐");

                self.print_spaced(byte, true);

                ops.push(a);
                ops.push(b);
                ops.push(c);
                ops.push(d);

                Ok(())
            };

            display_types(bytes.remove(0))?;

            if is_dvar {
                println!();
                display_types(bytes.remove(0))?;
            }
        }

        if ops[0] != OperandType::Omitted {
            println!()
        }

        for (i, op) in ops.iter().take_while(|op| **op != OperandType::Omitted).enumerate() {
            print!("Operand {i}: ");
            self.print_operand(*op, bytes);
        }

        Ok(())
    }

    fn print_spaced(&self, b: u8, newline: bool) {
        let binary = format!("{b:08b}");

        for c in binary.chars() {
            print!("{c} ");
        }

        if newline {
            println!();
        }
    }

    fn print_operand(&self, optype: OperandType, bytes: &mut Vec<u8>) {
        let first = bytes.remove(0);

        print!("{first:08b} ");

        match optype {
            OperandType::Large => {
                let second = u16::from(bytes.remove(0));

                let val = (u16::from(first) << 8) | second;

                if val >= 0x8000 {
                    println!("{:08b} ({} / {})", second, val, val.as_signed());
                } else {
                    println!("{second:08b} ({val})");
                }
            }
            OperandType::Small => println!("({first})"),
            OperandType::Variable => println!("({})", Arg::Variable(first)),
            OperandType::Omitted => println!("invalid type: {optype:?}"),
        }
    }
}
