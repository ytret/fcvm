use crate::data::*;
use crate::preproc;

#[derive(Debug, Clone, PartialEq)]
pub enum TokenType {
    Colon,
    Comma,
    OpenSqBr,
    CloseSqBr,
    ArithmOp(String),
    Identifier(String),
    Register(String),
    Number(i64),
    String(String),
    Comment(String),
}

pub type Token = Located<TokenType>;

#[derive(Debug)]
pub struct TokenizedLine {
    pub tokens: Vec<Token>,
    pub orig_line_num: usize,
}

#[derive(Debug)]
pub struct TokenizedSource {
    pub lines: Vec<TokenizedLine>,
    pub orig_lines: Vec<String>,
}

pub fn tokenize(source: preproc::PreprocSource) -> Result<TokenizedSource> {
    let mut tokenized_lines = Vec::new();

    for line in source.lines {
        let tokens = tokenize_line(&line, &source.orig_lines)?;
        tokenized_lines.push(TokenizedLine {
            tokens,
            orig_line_num: line.orig_line_num,
        });
    }

    Ok(TokenizedSource {
        lines: tokenized_lines,
        orig_lines: source.orig_lines,
    })
}

fn tokenize_line(line: &preproc::PreprocLine, orig_lines: &[String]) -> Result<Vec<Token>> {
    let mut tokens = Vec::new();
    let mut chars = line.content.char_indices().peekable();

    while let Some((start_pos, ch)) = chars.next() {
        let start_loc = SourceLoc {
            line: line.orig_line_num,
            column: line.start_col + start_pos + 1,
        };

        let mut push_single_char_token = |ty| {
            tokens.push(Located::new(
                ty,
                SourceSpan {
                    start: start_loc,
                    end: start_loc,
                },
            ))
        };

        match ch {
            ' ' | '\t' => continue,
            ':' => push_single_char_token(TokenType::Colon),
            ',' => push_single_char_token(TokenType::Comma),
            '[' => push_single_char_token(TokenType::OpenSqBr),
            ']' => push_single_char_token(TokenType::CloseSqBr),
            '+' | '-' => push_single_char_token(TokenType::ArithmOp(ch.to_string())),

            _ if ch.is_alphabetic() || ch == '_' || ch == '.' => {
                // Parse an identifier/register.
                let mut identifier = String::new();
                identifier.push(ch);
                let mut end_pos = start_pos;
                while let Some(&(pos, next_ch)) = chars.peek() {
                    if next_ch.is_alphanumeric() || next_ch == '_' || next_ch == '.' {
                        identifier.push(next_ch);
                        end_pos = pos;
                        chars.next();
                    } else {
                        break;
                    }
                }
                let end_loc = SourceLoc {
                    line: line.orig_line_num,
                    column: line.start_col + end_pos + 1,
                };
                let token_type = if identifier.to_lowercase().starts_with("r") {
                    TokenType::Register(identifier)
                } else {
                    TokenType::Identifier(identifier)
                };
                tokens.push(Located::new(
                    token_type,
                    SourceSpan {
                        start: start_loc,
                        end: end_loc,
                    },
                ));
            }

            _ if ch.is_ascii_digit() => {
                // Parse a decimal/hexadecimal number.
                let mut number_str = String::new();
                number_str.push(ch);
                let mut end_pos = start_pos;

                let is_hex: bool;
                if let Some(&(pos, next_ch)) = chars.peek() {
                    // If the number is hexadecimal, 'number_str' does not store
                    // the '0x' prefix, but rather only the digits that follow
                    // it.
                    is_hex = ch == '0' && next_ch == 'x';
                    if is_hex {
                        number_str.clear();
                    }
                    end_pos = pos;
                    chars.next();
                } else {
                    is_hex = false;
                };

                while let Some(&(pos, next_ch)) = chars.peek() {
                    if next_ch.is_alphanumeric() {
                        number_str.push(next_ch);
                        end_pos = pos;
                        chars.next();
                    } else {
                        break;
                    }
                }

                let end_loc = SourceLoc {
                    line: line.orig_line_num,
                    column: line.start_col + end_pos + 1,
                };
                match i64::from_str_radix(&number_str, if is_hex { 16 } else { 10 }) {
                    Ok(num) => tokens.push(Located::new(
                        TokenType::Number(num),
                        SourceSpan {
                            start: start_loc,
                            end: end_loc,
                        },
                    )),
                    Err(_) => {
                        return Err(AsmError::new(
                            format!(
                                "invalid {} number",
                                if is_hex { "hexadecimal" } else { "decimal" }
                            ),
                            SourceSpan {
                                start: start_loc,
                                end: end_loc,
                            },
                            orig_lines[line.orig_line_num - 1].clone(),
                        ));
                    }
                }
            }

            '"' => {
                // Parse a string.
                let mut str_val = String::new();
                let mut end_pos = start_pos;
                let mut escaped = false;

                while let Some(&(pos, next_ch)) = chars.peek() {
                    end_pos = pos;
                    chars.next();

                    if escaped {
                        escaped = false;
                        str_val.push(next_ch);
                    } else if next_ch == '\\' {
                        escaped = true;
                    } else if next_ch == '"' {
                        break;
                    } else {
                        str_val.push(next_ch);
                    }
                }

                let end_loc = SourceLoc {
                    line: line.orig_line_num,
                    column: line.start_col + end_pos + 1,
                };
                let src_span = SourceSpan {
                    start: start_loc,
                    end: end_loc,
                };

                if escaped {
                    return Err(AsmError::new(
                        format!("incomplete string literal"),
                        src_span,
                        orig_lines[line.orig_line_num - 1].clone(),
                    ));
                }

                tokens.push(Located::new(TokenType::String(str_val), src_span));
            }

            ';' => {
                // Comment until EOL.
                let mut comment = String::new();
                comment.push(ch);
                let mut end_pos = start_pos;
                while let Some(&(pos, next_ch)) = chars.peek() {
                    comment.push(next_ch);
                    end_pos = pos;
                    chars.next();
                }
                let end_loc = SourceLoc {
                    line: line.orig_line_num,
                    column: line.start_col + end_pos + 1,
                };
                tokens.push(Located::new(
                    TokenType::Comment(comment),
                    SourceSpan {
                        start: start_loc,
                        end: end_loc,
                    },
                ));
            }

            _ => {
                return Err(AsmError::new(
                    format!("unexpected character: {}", ch),
                    SourceSpan {
                        start: start_loc,
                        end: start_loc,
                    },
                    orig_lines[line.orig_line_num - 1].clone(),
                ));
            }
        }
    }

    Ok(tokens)
}
