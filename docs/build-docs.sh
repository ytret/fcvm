#!/usr/bin/env bash
#
# Renders the AsciiDoc manuals in docs/asciidoc to HTML and PDF.
#
# Usage:
#   nix develop path:. -c docs/build-docs.sh
#
# Environment variables:
#   DOCS_OUT_DIR  Output directory (default: docs/output, which is gitignored).
#   REVNUMBER     Revision to embed, overriding the git-derived value.
#   REVDATE       Revision date to embed, overriding the git-derived value.
#
# The revision shown on the title page is derived from git, so that it identifies
# the source the manual was built from rather than the time of the build:
#   - REVNUMBER is `git describe --tags --always --dirty`, that is, the nearest
#     tag plus commit count, or the abbreviated commit hash when no tags exist.
#   - REVDATE is the date of the last commit that changed docs/asciidoc.
#
# Runs `git describe` and `git log` in the repository; a bare `asciidoctor` run
# outside of this script falls back to the defaults in each document header.

set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
asciidoc_dir="$root_dir/docs/asciidoc"
out_dir="${DOCS_OUT_DIR:-$root_dir/docs/output}"
theme_file="$asciidoc_dir/adoc-theme.yml"
adoc_files=("$asciidoc_dir/ISA.adoc" "$asciidoc_dir/Peripherals.adoc")

for tool in asciidoctor asciidoctor-pdf; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "error: $tool not found; run this script inside the dev shell:" >&2
        echo "       nix develop path:. -c docs/build-docs.sh" >&2
        exit 1
    fi
done

if [[ -z "${REVNUMBER:-}" ]]; then
    REVNUMBER="$(git -C "$root_dir" describe --tags --always --dirty 2>/dev/null || echo unreleased)"
fi
if [[ -z "${REVDATE:-}" ]]; then
    REVDATE="$(git -C "$root_dir" log -1 --format=%cs -- docs/asciidoc 2>/dev/null || echo "")"
    REVDATE="${REVDATE:-$(date +%F)}"
fi

# Fail the build if Asciidoctor reports anything at WARN level or above, so that
# broken cross-references and malformed tables do not pass unnoticed.
asciidoctor_flags=(
    -b html5
    --failure-level=WARN
    -a "revnumber=$REVNUMBER"
    -a "revdate=$REVDATE"
)
pdf_flags=(
    --failure-level=WARN
    -a "pdf-theme=$theme_file"
    -a "revnumber=$REVNUMBER"
    -a "revdate=$REVDATE"
)

echo "fcvm docs: rev $REVNUMBER, $REVDATE"
mkdir -p "$out_dir"

echo "fcvm docs: HTML"
asciidoctor "${asciidoctor_flags[@]}" -D "$out_dir" "${adoc_files[@]}"

for adoc_file in "${adoc_files[@]}"; do
    name="$(basename "${adoc_file%.adoc}")"
    echo "fcvm docs: PDF  $name.pdf"
    asciidoctor-pdf "${pdf_flags[@]}" -o "$out_dir/$name.pdf" "$adoc_file"
done

echo "fcvm docs: written to $out_dir"
