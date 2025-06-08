use either::Either;
use phf::phf_map;
use std::collections::HashMap;
use std::iter::zip;

use crate::data::*;
use crate::parser::*;

/// Descriptor of an operand. Similar to [OperandType], except holds no data.
///
/// A single operand may have several descriptors. For example, a [number] `123` is described
/// using `Imm8` and `Imm32`. If it fitted in 5 bits, it would also be `Imm5`. See
/// [OperandDescriptor::describe_number()] and [OperandDescriptor::describe_operand()].
///
/// [number]: OperandType::Number
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
    /// Returns a vector of [descriptors] of an operand.
    ///
    /// [descriptors]: OperandDescriptor
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

    /// Returns a vector of [descriptors] of a number `num`.
    ///
    /// The number is assigned a descriptor if it fits into the appropriate range (inclusive on
    /// both ends):
    ///
    /// - `Imm32`: from `0` to [`u32::MAX`] if `num` is positive, otherwise from [`i32::MIN`] to
    ///   [`i32::MAX`].
    /// - `Imm8`: from `0` to [`u8::MAX`] if `num` is positive, otherwise from [`i8::MIN`] to
    ///   [`i8::MAX`].
    /// - `Imm5`: from `0` to `31` if `num` is positive, otherwise from `-16` to `15`.
    ///
    /// These two facts are always true:
    ///
    /// - If a number fits into the smaller range, it also fits into the broader ranges.
    /// - At least one of `Imm5`, `Imm8` or `Imm32` is returned or an error is returned otherwise.
    ///
    /// [descriptors]: OperandDescriptor
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

    fn generate_bytecode(self, operand: &Operand, orig_lines: &[String]) -> Result<Vec<u8>> {
        let parse_reg_str = |reg_str: &String| match reg_str.to_lowercase().as_str() {
            "r0" => Ok(vec![0x00]),
            "r1" => Ok(vec![0x01]),
            "r2" => Ok(vec![0x02]),
            "r3" => Ok(vec![0x03]),
            "r4" => Ok(vec![0x04]),
            "r5" => Ok(vec![0x05]),
            "r6" => Ok(vec![0x06]),
            "r7" => Ok(vec![0x07]),
            "sp" => Ok(vec![0x08]),
            _ => Err(AsmError::new(
                "unrecognized register".to_string(),
                operand.span,
                orig_lines[operand.span.start.line - 1].clone(),
            )),
        };
        match self {
            Self::Register => {
                // TODO: use numbers instead of strings
                match &operand.item {
                    OperandType::Register(reg_str) => parse_reg_str(reg_str),
                    _ => unreachable!(),
                }
            }
            Self::Label => todo!(),
            Self::Imm5 | Self::Imm8 => match &operand.item {
                OperandType::Number(num_val) => Ok(vec![*num_val as u8]),
                _ => unreachable!(),
            },
            Self::Imm32 => match &operand.item {
                OperandType::Number(num_val) => Ok(dword_to_bytes(*num_val as u32)),
                _ => unreachable!(),
            },
            Self::MemoryImm32 => todo!(),
            Self::MemoryRegNoOffset => match &operand.item {
                OperandType::MemoryNoMath(sub_operand) => match *sub_operand.clone() {
                    OperandType::Register(reg_str) => parse_reg_str(&reg_str),
                    _ => unreachable!(),
                },
                _ => unreachable!(),
            },
            Self::MemoryRegOffset8 => match &operand.item {
                OperandType::MemoryWithMath { lhs, rhs, op } => {
                    let mut bytecode =
                        OperandDescriptor::Register.generate_bytecode(lhs, orig_lines)?;
                    let rhs_multiplier = match *op {
                        '+' => 1,
                        '-' => -1,
                        _ => unreachable!(),
                    };
                    if let OperandType::Number(rhs_val) = rhs.item {
                        bytecode.extend(vec![(rhs_multiplier * rhs_val) as u8]);
                        Ok(bytecode)
                    } else {
                        unreachable!()
                    }
                }
                _ => unreachable!(),
            },
            Self::MemoryRegOffset32 => todo!(),
            Self::MemoryRegOffsetReg => match &operand.item {
                OperandType::MemoryWithMath { lhs, rhs, op } => {
                    assert!(*op == '+');
                    let mut bytecode =
                        OperandDescriptor::Register.generate_bytecode(lhs, orig_lines)?;
                    let rhs_bytecode =
                        OperandDescriptor::Register.generate_bytecode(rhs, orig_lines)?;
                    bytecode.extend_from_slice(&rhs_bytecode);
                    Ok(bytecode)
                }
                _ => unreachable!(),
            },
            Self::String => todo!(),
        }
    }
}

#[derive(Debug, Clone)]
struct InstrDescriptor {
    opcode: u8,
    operands: &'static [OperandDescriptor],
    size: usize,
}

struct PseudoInstrDescriptor {
    operands: &'static [OperandDescriptor],
    fn_size: fn(&Instr, usize, String) -> Result<usize>,
    fn_generate_bytecode: fn(&ResolvedInstr, String) -> Result<Vec<u8>>,
}

impl std::fmt::Debug for PseudoInstrDescriptor {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("PseudoInstrDescriptor")
            .field("operands", &self.operands)
            .field("fn_size", &"<function omitted>")
            .finish()
    }
}

/// Resolved pseudo- or regular instruction.
///
/// An instruction is resolved when its address, size and descriptor are determined.
///
/// - Size is used to calculate the address of the next instruction.
/// - Address is used to resolve labels.
/// - Descriptor is then used to generate bytecode for the instruction.
///
/// See [resolve_instruction()].
#[derive(Debug, Clone)]
struct ResolvedInstr {
    addr: usize,
    instr: Instr,
    desc: Option<Either<&'static InstrDescriptor, &'static PseudoInstrDescriptor>>,
    size: usize,
}

/// Resolves `instructions` into a vector of [`ResolvedInstr`].
///
/// See [resolve_instruction()].
fn resolve_instructions(
    instructions: &[Instr],
    orig_lines: &[String],
) -> Result<Vec<ResolvedInstr>> {
    let mut resolved_instructions = Vec::new();

    let mut addr = 0;
    for instr in instructions {
        let resolved_instr = resolve_instruction(instr, addr, orig_lines)?;
        addr = resolved_instr.addr + resolved_instr.size;
        resolved_instructions.push(resolved_instr);
    }

    Ok(resolved_instructions)
}

/// Resolves `instr` into a [`ResolvedInstr`].
///
/// Arguments:
/// - `instr` is the instruction to be resolved.
/// - `addr` is the address to put the instruction at.
/// - `orig_lines` is the original source text.
///
/// See [resolve_pseudo_instruction()] and [resolve_regular_instruction()].
fn resolve_instruction(instr: &Instr, addr: usize, orig_lines: &[String]) -> Result<ResolvedInstr> {
    if instr.item.mnemonic.is_some() {
        if instr.item.mnemonic.as_ref().unwrap().starts_with(".") {
            resolve_pseudo_instruction(instr, addr, orig_lines)
        } else {
            resolve_regular_instruction(instr, addr, orig_lines)
        }
    } else {
        Ok(ResolvedInstr {
            addr,
            instr: instr.clone(),
            desc: None,
            size: 0,
        })
    }
}

/// Resolves a pseudo-instruction into [`ResolvedInstr`].
///
/// See [`PseudoInstrDescriptor`] and [`PSEUDOS`].
fn resolve_pseudo_instruction(
    instr: &Instr,
    addr: usize,
    orig_lines: &[String],
) -> Result<ResolvedInstr> {
    let instr_mnemonic = instr.item.mnemonic.as_ref().unwrap();
    let instr_line = orig_lines[instr.span.start.line - 1].clone();

    let operand_descriptors = instr
        .item
        .operands
        .iter()
        .map(|operand| OperandDescriptor::describe_operand(operand, orig_lines))
        .collect::<Result<Vec<_>>>()?;

    if PSEUDOS.contains_key(instr_mnemonic) {
        let pseudo_descriptor: &PseudoInstrDescriptor = &PSEUDOS[instr_mnemonic];
        let correct_operands = pseudo_descriptor.operands.len() == operand_descriptors.len()
            && zip(pseudo_descriptor.operands, &operand_descriptors)
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
            Ok(ResolvedInstr {
                addr,
                instr: instr.clone(),
                desc: Some(Either::Right(pseudo_descriptor)),
                size: (pseudo_descriptor.fn_size)(instr, addr, instr_line)?,
            })
        } else {
            Err(AsmError::new(
                format!("bad operands for pseudo-instruction '{}'", instr_mnemonic),
                instr.span,
                instr_line,
            ))
        }
    } else {
        Err(AsmError::new(
            format!("unrecognized pseudo-instruction '{}'", instr_mnemonic),
            instr.span,
            instr_line,
        ))
    }
}

/// Resolves a regular instruction into [`ResolvedInstr`].
///
/// Matches an instruction to its [descriptor] by:
///
/// - Mnemonic.
/// - Types of operands.
///
/// [descriptor]: InstrDescriptor
fn resolve_regular_instruction(
    instr: &Instr,
    addr: usize,
    orig_lines: &[String],
) -> Result<ResolvedInstr> {
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

    let mut found_descriptor = None;
    for opcode_descriptor in opcodes.unwrap() {
        let correct_operands = opcode_descriptor.operands.len() == operand_descriptors.len()
            && zip(opcode_descriptor.operands, &operand_descriptors)
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
            found_descriptor = Some(opcode_descriptor);
            break;
        }
    }

    if found_descriptor.is_some() {
        Ok(ResolvedInstr {
            addr,
            instr: instr.clone(),
            desc: Some(Either::Left(found_descriptor.unwrap())),
            size: found_descriptor.unwrap().size,
        })
    } else {
        Err(AsmError::new(
            "unrecognized instruction, check the operands".to_string(),
            instr.span,
            orig_lines[instr.span.start.line - 1].clone(),
        ))
    }
}

/// Replaces each [identifier operand] in `resolved_instructions` with the label address.
///
/// - Maps label names to their addresses among the resolved instructions.
/// - Replaces each [identifier operand] with a [number operand]. The number value is the label's
///   address. If the label is not found, produces an error. After the function succeeds, there are
///   no identifier operands left.
/// - Label operands of relative control flow instructions are resolved relative to the instruction
///   address. In other cases, the label is resolved to an absolute address.
///
/// [identifier operand]: OperandType::Identifier
/// [number operand]: OperandType::Number
fn resolve_labels(
    resolved_instructions: &mut [ResolvedInstr],
    orig_lines: &[String],
) -> Result<()> {
    let mut resolved_labels = HashMap::new();
    for resolved_instr in resolved_instructions.as_ref() {
        if let Some(label_name) = &resolved_instr.instr.item.label {
            resolved_labels.insert(label_name.clone(), resolved_instr.addr);
        }
    }

    for resolved_instr in resolved_instructions {
        for operand in &mut resolved_instr.instr.item.operands {
            if let OperandType::Identifier(id_val) = &operand.item {
                if resolved_labels.contains_key(id_val) {
                    let label_addr = resolved_labels[id_val] as i64;
                    operand.item = OperandType::Number(label_addr);
                } else {
                    return Err(AsmError::new(
                        format!("unresolved label '{}'", id_val),
                        operand.span,
                        orig_lines[operand.span.start.line - 1].clone(),
                    ));
                }
            }
        }
    }

    Ok(())
}

fn generate_bytecode(
    resolved_instructions: &[ResolvedInstr],
    orig_lines: &[String],
) -> Result<Vec<u8>> {
    let mut bytecode = Vec::new();
    for resolved_instr in resolved_instructions {
        if let Some(instr_desc) = resolved_instr.desc {
            let instr_bytecode = if instr_desc.is_left() {
                generate_regular_instruction(resolved_instr, orig_lines)?
            } else {
                generate_pseudo_instruction(resolved_instr, orig_lines)?
            };
            bytecode.extend_from_slice(&instr_bytecode);
        }
    }
    Ok(bytecode)
}

fn generate_regular_instruction(
    resolved_instr: &ResolvedInstr,
    orig_lines: &[String],
) -> Result<Vec<u8>> {
    assert!(resolved_instr.desc.is_some_and(|desc| desc.is_left()));
    let instr_descriptor = resolved_instr.desc.unwrap().unwrap_left();
    let operand_descriptors = instr_descriptor.operands;

    let mut bytecode = vec![instr_descriptor.opcode];
    for (idx, operand_descriptor) in operand_descriptors.iter().enumerate() {
        let operand = resolved_instr.instr.item.operands.get(idx).unwrap();
        let operand_bytecode = operand_descriptor.generate_bytecode(operand, orig_lines)?;
        bytecode.extend_from_slice(&operand_bytecode);
    }
    Ok(bytecode)
}

fn generate_pseudo_instruction(
    resolved_instr: &ResolvedInstr,
    orig_lines: &[String],
) -> Result<Vec<u8>> {
    assert!(resolved_instr.desc.is_some_and(|desc| desc.is_right()));
    let pseudo_descriptor = resolved_instr.desc.unwrap().unwrap_right();
    (pseudo_descriptor.fn_generate_bytecode)(
        &resolved_instr,
        orig_lines[resolved_instr.instr.span.start.line - 1].clone(),
    )
}

fn dword_to_bytes(dword: u32) -> Vec<u8> {
    vec![
        ((dword >> 0) & 0xFF) as u8,
        ((dword >> 8) & 0xFF) as u8,
        ((dword >> 16) & 0xFF) as u8,
        ((dword >> 24) & 0xFF) as u8,
    ]
}

pub fn codegen(parsed_prog: &ParsedProgram) -> Result<Vec<u8>> {
    let mut resolved_instructions =
        resolve_instructions(&parsed_prog.instructions, &parsed_prog.orig_lines)?;
    resolve_labels(&mut resolved_instructions, &parsed_prog.orig_lines)?;
    generate_bytecode(&resolved_instructions, &parsed_prog.orig_lines)
}

static OPCODES: phf::Map<&'static str, &[InstrDescriptor]> = phf_map! {
    "mov" => &[
        InstrDescriptor {
            opcode: 0x20,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Register],
            size: 2,
        },
        InstrDescriptor {
            opcode: 0x21,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Imm32],
            size: 6,
        },
    ],
    "str" => &[
        InstrDescriptor {
            opcode: 0x23,
            operands: &[OperandDescriptor::MemoryImm32, OperandDescriptor::Register],
            size: 6,
        },
        InstrDescriptor {
            opcode: 0x22,
            operands: &[OperandDescriptor::MemoryRegNoOffset, OperandDescriptor::Register],
            size: 2,
        },
        InstrDescriptor {
            opcode: 0x24,
            operands: &[OperandDescriptor::MemoryRegOffset8, OperandDescriptor::Register],
            size: 3,
        },
        InstrDescriptor {
            opcode: 0x25,
            operands: &[OperandDescriptor::MemoryRegOffset32, OperandDescriptor::Register],
            size: 6,
        },
        InstrDescriptor {
            opcode: 0x26,
            operands: &[OperandDescriptor::MemoryRegOffsetReg, OperandDescriptor::Register],
            size: 3,
        },
    ],
    "ldr" => &[
        InstrDescriptor {
            opcode: 0x27,
            operands: &[OperandDescriptor::Register, OperandDescriptor::MemoryImm32],
            size: 6,
        },
        InstrDescriptor {
            opcode: 0x28,
            operands: &[OperandDescriptor::Register, OperandDescriptor::MemoryRegNoOffset],
            size: 2,
        },
        InstrDescriptor {
            opcode: 0x29,
            operands: &[OperandDescriptor::Register, OperandDescriptor::MemoryRegOffset8],
            size: 3,
        },
        InstrDescriptor {
            opcode: 0x2A,
            operands: &[OperandDescriptor::Register, OperandDescriptor::MemoryRegOffset32],
            size: 6,
        },
        InstrDescriptor {
            opcode: 0x2B,
            operands: &[OperandDescriptor::Register, OperandDescriptor::MemoryRegOffsetReg],
            size: 3,
        },
    ],

    "add" => &[
        InstrDescriptor {
            opcode: 0x42,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Register],
            size: 2,
        },
        InstrDescriptor {
            opcode: 0x41,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Imm32],
            size: 6,
        },
    ],
    "sub" => &[
        InstrDescriptor {
            opcode: 0x44,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Register],
            size: 2,
        },
        InstrDescriptor {
            opcode: 0x43,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Imm32],
            size: 6,
        },
    ],
    "not" => &[
        InstrDescriptor {
            opcode: 0x57,
            operands: &[OperandDescriptor::Register],
            size: 2,
        }
    ],
    "and" => &[
        InstrDescriptor {
            opcode: 0x4C,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Register],
            size: 2,
        },
        InstrDescriptor {
            opcode: 0x4B,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Imm32],
            size: 6,
        },
    ],
    "shr" => &[
        InstrDescriptor {
            opcode: 0x54,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Register],
            size: 2,
        },
        InstrDescriptor {
            opcode: 0x53,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Imm5],
            size: 3,
        },
    ],
    "cmp" => &[
        InstrDescriptor {
            opcode: 0x5A,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Register],
            size: 2,
        }
    ],
    "tst" => &[
        InstrDescriptor {
            opcode: 0x5C,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Register],
            size: 2,
        },
        InstrDescriptor {
            opcode: 0x55,
            operands: &[OperandDescriptor::Register, OperandDescriptor::Imm32],
            size: 6,
        },
    ],

    "jmpr" => &[
        InstrDescriptor {
            opcode: 0x60,
            operands: &[OperandDescriptor::Imm8],
            size: 2,
        }
    ],
    "jmpa" => &[
        InstrDescriptor {
            opcode: 0x61,
            operands: &[OperandDescriptor::Imm32],
            size: 5,
        },
        InstrDescriptor {
            opcode: 0x62,
            operands: &[OperandDescriptor::Register],
            size: 2,
        },
    ],
    "jeqr" => &[
        InstrDescriptor {
            opcode: 0x64,
            operands: &[OperandDescriptor::Imm8],
            size: 2,
        }
    ],
    "jner" => &[
        InstrDescriptor {
            opcode: 0x68,
            operands: &[OperandDescriptor::Imm8],
            size: 2,
        }
    ],
    "call" => &[
        InstrDescriptor {
            opcode: 0x7D,
            operands: &[OperandDescriptor::Imm32],
            size: 5,
        },
        InstrDescriptor {
            opcode: 0x7E,
            operands: &[OperandDescriptor::Register],
            size: 2,
        },
    ],
    "ret" => &[InstrDescriptor { opcode: 0x7F, operands: &[], size: 1}],

    "halt" => &[InstrDescriptor { opcode: 0xA1, operands: &[], size: 1 }],
    "iret" => &[InstrDescriptor { opcode: 0xA3, operands: &[], size: 1 }],
};

static PSEUDOS: phf::Map<&'static str, PseudoInstrDescriptor> = phf_map! {
    ".origin" => PseudoInstrDescriptor {
        operands: &[OperandDescriptor::Imm32],
        fn_size: |instr: &Instr, addr: usize, instr_line: String| -> Result<usize> {
            if let OperandType::Number(new_addr) = instr.item.operands[0].item {
                if new_addr >= 0 && new_addr as usize >= addr {
                    Ok(new_addr as usize - addr)
                } else {
                    Err(AsmError::new(
                        format!("address must be greater than or equal to {}", addr),
                        instr.item.operands[0].span,
                        instr_line,
                    ))
                }
            } else {
                unreachable!()
            }
        },
        fn_generate_bytecode: |resolved_instr, _instr_line| -> Result<Vec<u8>> {
            Ok(vec![0; resolved_instr.size])
        },
    },
    ".skip" => PseudoInstrDescriptor {
        operands: &[OperandDescriptor::Imm32],
        fn_size: |instr: &Instr, _: usize, instr_line: String| -> Result<usize> {
            if let OperandType::Number(num_bytes) = instr.item.operands[0].item {
                if num_bytes >= 0 {
                    Ok(num_bytes as usize)
                } else {
                    Err(AsmError::new(
                        "expected a positive number".to_string(),
                        instr.item.operands[0].span,
                        instr_line,
                    ))
                }
            } else {
                unreachable!()
            }
        },
        fn_generate_bytecode: |_resolved_instr, _instr_line| -> Result<Vec<u8>> {
            todo!()
        },
    },

    ".strd" => PseudoInstrDescriptor {
        operands: &[OperandDescriptor::Imm32],
        fn_size: |_: &Instr, _: usize, _: String| -> Result<usize> {
            Ok(4)
        },
        fn_generate_bytecode: |resolved_instr, _instr_line| -> Result<Vec<u8>> {
            if let OperandType::Number(num_val) = resolved_instr.instr.item.operands[0].item {
                // num_val fits into u32, see OperandDescriptor::describe_number().
                Ok(dword_to_bytes(num_val as u32))
            } else {
                unreachable!()
            }
        },
    },
    ".strb" => PseudoInstrDescriptor {
        operands: &[OperandDescriptor::Imm8],
        fn_size: |_: &Instr, _: usize, _: String| -> Result<usize> {
            Ok(1)
        },
        fn_generate_bytecode: |resolved_instr, _instr_line| -> Result<Vec<u8>> {
            if let OperandType::Number(num_val) = resolved_instr.instr.item.operands[0].item {
                Ok(vec![num_val as u8])
            } else {
                unreachable!()
            }
        },
    },
    ".strs" => PseudoInstrDescriptor {
        operands: &[OperandDescriptor::String],
        fn_size: |instr: &Instr, _: usize, _: String| -> Result<usize> {
            match &instr.item.operands[0].item {
                OperandType::String(str_val) => {
                    Ok(str_val.len())
                }
                _ => unreachable!(),
            }
        },
        fn_generate_bytecode: |resolved_instr, _instr_line| -> Result<Vec<u8>> {
            if let OperandType::String(str_val) = &resolved_instr.instr.item.operands[0].item {
                Ok(str_val.clone().into_bytes())
            } else {
                unreachable!()
            }
        },
    },
};
