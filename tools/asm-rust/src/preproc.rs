#[derive(Debug, Clone)]
pub struct PreprocLine {
    pub content: String,
    pub orig_line_num: usize,
    pub start_col: usize,
}

#[derive(Debug, Clone)]
pub struct PreprocSource {
    pub lines: Vec<PreprocLine>,
    pub orig_lines: Vec<String>,
}

pub fn preprocess(text: &str) -> PreprocSource {
    let orig_lines: Vec<String> = text.lines().map(|s| s.to_string()).collect();
    let mut preproc_lines = Vec::new();

    for (line_num, line) in orig_lines.iter().enumerate() {
        let trimmed = line.trim();

        // Skip empty lines and comments.
        if !trimmed.is_empty() && !trimmed.starts_with(';') {
            let start_col = line
                .chars()
                .position(|c| c == trimmed.chars().next().unwrap())
                .unwrap();

            preproc_lines.push(PreprocLine {
                content: trimmed.to_string(),
                orig_line_num: line_num + 1,
                start_col,
            });
        }
    }

    PreprocSource {
        lines: preproc_lines,
        orig_lines,
    }
}
