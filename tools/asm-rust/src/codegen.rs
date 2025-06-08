use phf::phf_map;
use std::iter::zip;

use crate::data::*;
use crate::parser::*;

#[derive(Debug, Clone, Copy)]
enum OperandCodec {
    Register,
    RegisterSrcDest,
    Imm5,
    Imm8,
    Imm32,
}

#[derive(Debug, Clone, Copy, PartialEq)]
enum OperandDescriptor {
    Register,
    Label,
    Imm5,
    Imm8,
    Imm32,
    MemoryImm32,
    MemoryRegNoOffset,
    MemoryRegOffset8,
    MemoryRegOffset32,
    MemoryRegOffsetReg,
    String,
}

impl OperandDescriptor {
    fn describe_operand(
        operand: &Operand,
        orig_lines: &[String],
    ) -> Result<Vec<OperandDescriptor>> {
        match &operand.item {
            OperandType::Register(_) => Ok(vec![Self::Register]),
            OperandType::Identifier(_) => Ok(vec![Self::Label]),
            OperandType::Number(num) => {
                OperandDescriptor::describe_number(*num, operand.span, orig_lines)
            }
            OperandType::MemoryNoMath(sub_operand) => match **sub_operand {
                OperandType::Number(_) => Ok(vec![OperandDescriptor::MemoryImm32]),
                OperandType::Register(_) => Ok(vec![OperandDescriptor::MemoryRegNoOffset]),
                _ => unreachable!(),
            },
            OperandType::MemoryWithMath { lhs, rhs, op: _ } => {
                assert!(matches!(lhs.item, OperandType::Register(_)));
                match rhs.item {
                    OperandType::Register(_) => Ok(vec![OperandDescriptor::MemoryRegOffsetReg]),
                    OperandType::Number(num) => {
                        let mut mem_descriptors = Vec::new();
                        let rhs_descriptors = Self::describe_number(num, rhs.span, orig_lines)?;
                        if rhs_descriptors.contains(&OperandDescriptor::Imm8) {
                            mem_descriptors.push(OperandDescriptor::MemoryRegOffset8);
                        }
                        if rhs_descriptors.contains(&OperandDescriptor::Imm32) {
                            mem_descriptors.push(OperandDescriptor::MemoryRegOffset32);
                        }
                        Ok(mem_descriptors)
                    }
                    _ => unreachable!(),
                }
            }
            OperandType::String(_) => Ok(vec![OperandDescriptor::String]),
        }
    }

    fn describe_number(
        num: i64,
        num_span: SourceSpan,
        orig_lines: &[String],
    ) -> Result<Vec<OperandDescriptor>> {
        let mut descriptors = Vec::new();

        let imm5_range = if num >= 0 { (0, 31) } else { (-16, 15) };
        let imm8_range = if num >= 0 { (0, 255) } else { (-128, 127) };
        let imm32_range = if num >= 0 {
            (0, u32::MAX as i64)
        } else {
            (i32::MIN as i64, i32::MAX as i64)
        };
        if imm5_range.0 <= num && num <= imm5_range.1 {
            descriptors.push(OperandDescriptor::Imm5);
        }
        if imm8_range.0 <= num && num <= imm8_range.1 {
            descriptors.push(OperandDescriptor::Imm8);
        }
        if imm32_range.0 <= num && num <= imm32_range.1 {
            descriptors.push(OperandDescriptor::Imm32);
        }

        if descriptors.is_empty() {
            Err(AsmError::new(
                "number is out of imm32 range".to_string(),
                num_span,
                orig_lines[num_span.start.line - 1].clone(),
            ))
        } else {
            Ok(descriptors)
        }
    }
}

#[derive(Debug, Clone)]
struct OpcodeDescriptor {
    opcode: u8,
    operands: &'static [OperandDescriptor],
}

#[derive(Debug, Clone)]
struct ResolvedInstr {
    instr: Instr,
    opcode_desc: OpcodeDescriptor,
}

static OPCODES: phf::Map<&'static str, &[OpcodeDescriptor]> = phf_map! {
    "mov" => &[
        OpcodeDescriptor {
            opcode: 0x20,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Register],
        },
        OpcodeDescriptor {
            opcode: 0x21,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Imm32],
        },
    ],
    "str" => &[
        OpcodeDescriptor {
            opcode: 0x23,
            operands: &[OperandDescriptor::MemoryImm32, OperandDescriptor::Register],
        },
        OpcodeDescriptor {
            opcode: 0x22,
            operands: &[OperandDescriptor::MemoryRegNoOffset, OperandDescriptor::Register],
        },
        OpcodeDescriptor {
            opcode: 0x24,
            operands: &[OperandDescriptor::MemoryRegOffset8, OperandDescriptor::Register],
        },
        OpcodeDescriptor {
            opcode: 0x25,
            operands: &[OperandDescriptor::MemoryRegOffset32, OperandDescriptor::Register],
        },
        OpcodeDescriptor {
            opcode: 0x26,
            operands: &[OperandDescriptor::MemoryRegOffsetReg, OperandDescriptor::Register],
        },
    ],
    "ldr" => &[
        OpcodeDescriptor {
            opcode: 0x27,
            operands: &[OperandDescriptor::Register, OperandDescriptor::MemoryImm32],
        },
        OpcodeDescriptor {
            opcode: 0x28,
            operands: &[OperandDescriptor::Register, OperandDescriptor::MemoryRegNoOffset],
        },
        OpcodeDescriptor {
            opcode: 0x29,
            operands: &[OperandDescriptor::Register, OperandDescriptor::MemoryRegOffset8],
        },
        OpcodeDescriptor {
            opcode: 0x2A,
            operands: &[OperandDescriptor::Register, OperandDescriptor::MemoryRegOffset32],
        },
        OpcodeDescriptor {
            opcode: 0x2B,
            operands: &[OperandDescriptor::Register, OperandDescriptor::MemoryRegOffsetReg],
        },
    ],

    "add" => &[
        OpcodeDescriptor {
            opcode: 0x42,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Register],
        },
        OpcodeDescriptor {
            opcode: 0x41,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Imm32],
        },
    ],
    "sub" => &[
        OpcodeDescriptor {
            opcode: 0x44,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Register],
        },
        OpcodeDescriptor {
            opcode: 0x43,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Imm32],
        },
    ],
    "not" => &[OpcodeDescriptor { opcode: 0x57, operands: &[OperandDescriptor::Register] }],
    "and" => &[
        OpcodeDescriptor {
            opcode: 0x4C,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Register],
        },
        OpcodeDescriptor {
            opcode: 0x4B,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Imm32],
        },
    ],
    "shr" => &[
        OpcodeDescriptor {
            opcode: 0x54,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Register],
        },
        OpcodeDescriptor {
            opcode: 0x53,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Imm5],
        },
    ],
    "cmp" => &[
        OpcodeDescriptor {
            opcode: 0x5A,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Register],
        }
    ],
    "tst" => &[
        OpcodeDescriptor {
            opcode: 0x5C,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Register],
        },
        OpcodeDescriptor {
            opcode: 0x55,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Imm32],
        },
    ],

    "jmpr" => &[OpcodeDescriptor { opcode: 0x60, operands: &[OperandDescriptor::Imm8] }],
    "jmpa" => &[
        OpcodeDescriptor {
            opcode: 0x61,
            operands: &[OperandDescriptor::Imm32],
        },
        OpcodeDescriptor {
            opcode: 0x62,
            operands: &[OperandDescriptor::Register],
        },
    ],
    "jeqr" => &[OpcodeDescriptor { opcode: 0x64, operands: &[OperandDescriptor::Imm8] }],
    "jner" => &[OpcodeDescriptor { opcode: 0x68, operands: &[OperandDescriptor::Imm8] }],
    "call" => &[
        OpcodeDescriptor {
            opcode: 0x7D,
            operands: &[OperandDescriptor::Imm32],
        },
        OpcodeDescriptor {
            opcode: 0x7E,
            operands: &[OperandDescriptor::Register],
        },
    ],
    "ret" => &[OpcodeDescriptor { opcode: 0x7F, operands: &[] }],

    "halt" => &[OpcodeDescriptor { opcode: 0xA1, operands: &[] }],
    "iret" => &[OpcodeDescriptor { opcode: 0xA3, operands: &[] }],
};

pub fn codegen(parsed_prog: &ParsedProgram) -> Result<()> {
    for instr in &parsed_prog.instructions[..] {
        if instr.item.mnemonic.is_none() || instr.item.mnemonic.as_ref().unwrap().starts_with(".") {
            continue;
        }
        let resolved_instr = resolve_instruction(&instr, &parsed_prog.orig_lines)?;
    }
    Ok(())
}

fn resolve_instruction(instr: &Instr, orig_lines: &[String]) -> Result<ResolvedInstr> {
    assert!(instr.item.mnemonic.is_some());
    assert!(!instr.item.mnemonic.as_ref().unwrap().starts_with("."));

    let mut opcodes = None;
    for (mnemonic, it_opcodes) in &OPCODES {
        if instr.item.mnemonic.as_ref().unwrap() == *mnemonic {
            opcodes = Some(*it_opcodes);
        }
    }

    if opcodes.is_none() {
        return Err(AsmError::new(
            "unrecognized instruction mnemonic".to_string(),
            instr.span,
            orig_lines[instr.span.start.line - 1].clone(),
        ));
    }

    let operand_descriptors = instr
        .item
        .operands
        .iter()
        .map(|operand| OperandDescriptor::describe_operand(operand, orig_lines))
        .collect::<Result<Vec<_>>>()?;

    let mut found_opcode = None;
    for opcode_descriptor in opcodes.unwrap() {
        let correct_operands = zip(opcode_descriptor.operands, &operand_descriptors)
            .map(|(expected_operand, actual_operands)| {
                // HACK: at this stage label operands are not yet resolved and have the type
                // 'Operand'; here we treat them as [Imm8 Imm32].
                if let [OperandDescriptor::Label] = &actual_operands[..] {
                    *expected_operand == OperandDescriptor::Imm8
                        || *expected_operand == OperandDescriptor::Imm32
                } else {
                    actual_operands.contains(expected_operand)
                }
            })
            .fold(true, |acc, operand_matches| acc & operand_matches);
        if correct_operands {
            found_opcode = Some(opcode_descriptor);
            break;
        }
    }

    if found_opcode.is_some() {
        Ok(ResolvedInstr {
            instr: instr.clone(),
            opcode_desc: found_opcode.unwrap().clone(),
        })
    } else {
        Err(AsmError::new(
            "unrecognized instruction, check the operands".to_string(),
            instr.span,
            orig_lines[instr.span.start.line - 1].clone(),
        ))
    }
}
