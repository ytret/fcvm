#[derive(Debug, Clone, Copy, PartialEq)]
pub struct SourceLoc {
    pub line: usize,
    pub column: usize,
}

#[derive(Debug, Clone, Copy)]
pub struct SourceSpan {
    pub start: SourceLoc,
    pub end: SourceLoc,
}

#[derive(Debug, Clone)]
pub struct Located<T> {
    pub item: T,
    pub span: SourceSpan,
}

impl<T> Located<T> {
    pub fn new(item: T, span: SourceSpan) -> Self {
        Self { item, span }
    }

    pub fn map<U>(self, f: impl FnOnce(T) -> U) -> Located<U> {
        Located {
            item: f(self.item),
            span: self.span,
        }
    }
}

#[derive(Debug, Clone)]
pub struct AsmError {
    pub msg: String,
    pub span: SourceSpan,
    pub src_line: String, // The actual source line for display.
}

impl AsmError {
    pub fn new(msg: String, span: SourceSpan, src_line: String) -> Self {
        Self {
            msg,
            span,
            src_line,
        }
    }
}

impl std::fmt::Display for AsmError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let line_num = self.span.start.line;
        let start_col = self.span.start.column;
        let end_col = self.span.end.column;
        writeln!(
            f,
            "Error at line {}, columns {}..{}: {}",
            line_num, start_col, end_col, self.msg
        )?;
        writeln!(f, "{} | {}", line_num, self.src_line)?;

        let line_num_digits = (line_num as f64).log10().floor() as usize + 1;
        let pointer_line =
            " ".repeat(line_num_digits + 3 + start_col - 1) + &"^".repeat(end_col - start_col + 1);
        write!(f, "{}", pointer_line)?;

        Ok(())
    }
}

impl std::error::Error for AsmError {}

pub type Result<T> = std::result::Result<T, AsmError>;
