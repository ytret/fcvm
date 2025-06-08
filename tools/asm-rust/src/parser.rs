use std::collections::HashMap;

use crate::data::*;
use crate::scanner::*;

/// Type of an instruction operand or memory accesss sub-operand.
#[derive(Debug, Clone)]
pub enum OperandType {
    Register(String),
    Identifier(String),
    Number(i64),
    MemoryNoMath(Box<OperandType>),
    MemoryWithMath {
        lhs: Box<OperandType>,
        rhs: Box<OperandType>,
        op: char,
    },
    String(String),
}

pub type Operand = Located<OperandType>;

#[derive(Debug, Clone)]
pub struct InstrItem {
    label: Option<String>,
    mnemonic: Option<String>,
    operands: Vec<Operand>,
}

pub type Instr = Located<InstrItem>;

#[derive(Debug)]
pub struct ParsedProgram {
    pub instructions: Vec<Instr>,
    pub variables: HashMap<String, OperandType>,
    pub labels: Vec<String>,
    pub orig_lines: Vec<String>,
}

#[derive(Debug)]
struct ProgramParser {
    prog: ParsedProgram,
}

impl ProgramParser {
    fn new() -> Self {
        Self {
            prog: ParsedProgram {
                instructions: Vec::new(),
                variables: HashMap::new(),
                labels: Vec::new(),
                orig_lines: Vec::new(),
            },
        }
    }

    /// Creates an instructions vector for token-lines in `tok_src`.
    fn parse_tokens(mut self: Self, tok_src: TokenizedSource) -> Result<Self> {
        self.prog.orig_lines = tok_src.orig_lines;

        for tok_line in &tok_src.lines {
            let instr = self.parse_token_line(tok_line.clone())?;
            if instr.item.label.is_some() {
                self.prog.labels.push(instr.item.label.clone().unwrap());
            }
            self.prog.instructions.push(instr);
        }

        Ok(self)
    }

    /// Creates an instruction for token-line `tok_line`.
    ///
    /// `tok_line` must have the following format:
    /// ```
    /// - [id colon] id        [opd1       [comma opd2 [comma ...]]]
    ///   ^label     ^mnemonic  ^operand 1 ^operand 2
    /// ```
    ///
    /// Operand tokens are separated by comma tokens and are parsed using
    /// [ProgramParser::parse_operand_tokens()] into a vector of [Operand].
    fn parse_token_line(&self, tok_line: TokenizedLine) -> Result<Instr> {
        let label = match tok_line
            .tokens
            .iter()
            .map(|tok| &tok.item)
            .collect::<Vec<_>>()[..]
        {
            [TokenType::Identifier(label_name), TokenType::Colon, ..] => Some(label_name.clone()),
            _ => None,
        };

        let mnemonic_idx = if label.is_some() { 2 } else { 0 };
        let has_mnemonic = tok_line.tokens.len() > mnemonic_idx;
        let mut mnemonic = None;
        if has_mnemonic {
            let mnemonic_tok = tok_line.tokens[mnemonic_idx].clone();
            if let TokenType::Identifier(mnemonic_val) = mnemonic_tok.item {
                mnemonic = Some(mnemonic_val);
            } else {
                return Err(AsmError::new(
                    format!("invalid instruction mnemonic"),
                    mnemonic_tok.span,
                    self.prog.orig_lines[tok_line.orig_line_num - 1].clone(),
                ));
            }
        }

        let mut operand_tokens: Vec<Vec<Token>> = Vec::new();
        let mut curr_operand_tokens: Vec<Token> = Vec::new();
        if mnemonic_idx + 1 < tok_line.tokens.len() {
            for tok in &tok_line.tokens[mnemonic_idx + 1..] {
                if tok.item == TokenType::Comma {
                    if curr_operand_tokens.len() == 0 {
                        return Err(AsmError::new(
                            format!("unexpected comma"),
                            tok.span.clone(),
                            self.prog.orig_lines[tok_line.orig_line_num - 1].clone(),
                        ));
                    }
                    operand_tokens.push(curr_operand_tokens.clone());
                    curr_operand_tokens.clear();
                } else {
                    curr_operand_tokens.push(tok.clone());
                }
            }
            if curr_operand_tokens.len() > 0 {
                operand_tokens.push(curr_operand_tokens.clone());
            }
        }

        let mut operands: Vec<Operand> = Vec::new();
        for curr_operand_tokens in operand_tokens {
            operands.push(self.parse_operand_tokens(curr_operand_tokens)?);
        }

        let instr_span = SourceSpan {
            start: tok_line.tokens[0].span.start,
            end: tok_line.tokens.last().unwrap().span.end,
        };
        Ok(Located::new(
            InstrItem {
                label: label,
                mnemonic: mnemonic,
                operands: operands,
            },
            instr_span,
        ))
    }

    /// Parses tokens in `operand_tokens` into an [`Operand`].
    ///
    /// There are several [types of operands], each being represented by
    /// different [types of tokens]:
    ///
    /// - `Register` → [OperandType::Register]
    /// - `Identifier` → [OperandType::Identifier]
    /// - `Number` → [OperandType::Number]
    /// - `[Number]` or `[Identifier]` → [OperandType::MemoryNoMath]
    /// - `[Register ArithmOp Number]` or `[Register ArithmOp Register]` →
    ///   [OperandType::MemoryWithMath]
    /// - `String` → [OperandType::String]
    ///
    /// _Tokens `[` and `]` are `OpenSqBr` and `CloseSqBr`, respectively._
    ///
    /// [types of operands]: OperandType
    /// [types of tokens]: TokenType
    fn parse_operand_tokens(&self, operand_tokens: Vec<Token>) -> Result<Operand> {
        let operand_span = SourceSpan {
            start: operand_tokens[0].span.start,
            end: operand_tokens.last().unwrap().span.end,
        };
        let line_num = operand_span.start.line;
        let operand_type = match operand_tokens
            .iter()
            .map(|tok| &tok.item)
            .collect::<Vec<_>>()[..]
        {
            [TokenType::Identifier(id_str)] => OperandType::Identifier(id_str.clone()),
            [TokenType::Register(reg_str)] => OperandType::Register(reg_str.clone()),
            [TokenType::String(str_val)] => OperandType::String(str_val.clone()),
            [TokenType::Number(num_val)] => OperandType::Number(num_val.clone()),
            [
                TokenType::OpenSqBr,
                TokenType::Number(num_val),
                TokenType::CloseSqBr,
            ] => OperandType::MemoryNoMath(Box::new(OperandType::Number(num_val.clone()))),
            [
                TokenType::OpenSqBr,
                TokenType::Identifier(id_str),
                TokenType::CloseSqBr,
            ] => OperandType::MemoryNoMath(Box::new(OperandType::Identifier(id_str.clone()))),
            [
                TokenType::OpenSqBr,
                TokenType::Register(reg),
                TokenType::CloseSqBr,
            ] => OperandType::MemoryNoMath(Box::new(OperandType::Register(reg.clone()))),
            [
                TokenType::OpenSqBr,
                TokenType::Register(reg),
                TokenType::ArithmOp(arithm_op),
                TokenType::Number(offset),
                TokenType::CloseSqBr,
            ] => OperandType::MemoryWithMath {
                lhs: Box::new(OperandType::Register(reg.clone())),
                rhs: Box::new(OperandType::Number(offset.clone())),
                op: arithm_op.clone(),
            },
            [
                TokenType::OpenSqBr,
                TokenType::Register(reg_base),
                TokenType::ArithmOp(arithm_op),
                TokenType::Register(reg_offset),
                TokenType::CloseSqBr,
            ] => OperandType::MemoryWithMath {
                lhs: Box::new(OperandType::Register(reg_base.clone())),
                rhs: Box::new(OperandType::Register(reg_offset.clone())),
                op: arithm_op.clone(),
            },
            _ => {
                return Err(AsmError::new(
                    format!("bad operand tokens"),
                    operand_span,
                    self.prog.orig_lines[line_num - 1].clone(),
                ));
            }
        };
        Ok(Located::new(operand_type, operand_span))
    }

    /// Parses `.set` instructions.
    ///
    /// Each `.set` instruction must have two operands:
    /// 1. Variable name -- [OperandType::Identifier].
    /// 2. Variable value.
    ///
    /// Allowed variable value types:
    /// 1. [OperandType::Identifier] -- a label or another variable. If the
    ///    value is a previously defined variable name, it gets expanded.
    /// 2. [OperandType::Number].
    /// 3. [OperandType::String].
    ///
    /// All other operand types are prohibited and produce an [AsmError].
    ///
    /// Adds the variable name and value to the hashmap
    /// [ParsedProgram::variables].
    fn parse_setvars(mut self: Self) -> Result<Self> {
        fn parse_setvar(instr: Instr) -> Result<(String, OperandType)> {
            assert!(instr.item.mnemonic.unwrap() == ".set");
            todo!();
        }

        for instr in &self.prog.instructions[..] {
            if let Some(".set") = instr.item.mnemonic.as_deref() {
                let variable = parse_setvar(instr.clone());
            }
        }

        Ok(self)
    }

    fn expand_vars(mut self: Self) -> Result<Self> {
        todo!();
    }
}

pub fn parse(tok_src: TokenizedSource) -> Result<ParsedProgram> {
    Ok(ProgramParser::new()
        .parse_tokens(tok_src)?
        .parse_setvars()?
        .expand_vars()?
        .prog)
}
