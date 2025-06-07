use clap::Parser;
use std::fs;
use std::path::PathBuf;

mod data;
mod preproc;
mod scanner;
mod parser;

use data::*;

#[derive(Parser, Debug)]
#[command(version, about)]
struct Args {
    /// Input file path.
    input: PathBuf,

    /// Output file path.
    #[arg(short, long, default_value = "prog.bin")]
    output: PathBuf,
}

fn main() {
    let args = Args::parse();

    let in_path = args.input;
    let out_path = args.output;

    let in_str = in_path.clone().to_str().unwrap().to_owned();
    let out_str = out_path.clone().to_str().unwrap().to_owned();

    eprintln!("Input path: {}", in_str);
    eprintln!("Output path: {}", out_str);

    if in_path == out_path {
        eprintln!("Error: input and output files must differ.");
    }

    if let Ok(src_text) = fs::read_to_string(in_path) {
        match assemble(src_text) {
            Ok(_) => eprintln!("Success"),
            Err(e) => eprintln!("{}", e),
        }
    } else {
        eprintln!("Error: failed to read file '{}'", in_str);
    }
}

fn assemble(src_text: String) -> Result<()> {
    let preproc_src = preproc::preprocess(&src_text);
    let tok_src = scanner::tokenize(preproc_src)?;
    let parsed_prog = parser::parse(tok_src)?;
    eprintln!("{:#?}", parsed_prog);
    Ok(())
}
