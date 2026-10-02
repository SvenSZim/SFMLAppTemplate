#!/usr/bin/env sh
# Formats every C++ file in the repository with clang-format.
#
#   tools/format.sh           rewrite files in place
#   tools/format.sh --check   change nothing; fail if a file is not formatted
#
# CI uses clang-format 23.1.2 (pipx install clang-format==23.1.2). Other versions can format
# slightly differently. Set CLANG_FORMAT to use a specific binary.
set -eu

cd "$(dirname "$0")/.."
clang_format="${CLANG_FORMAT:-clang-format}"

# Tracked files and new files that are not ignored: a file must not escape the check
# just because it has not been added to git yet.
list_sources() {
    git ls-files -z --cached --others --exclude-standard '*.hpp' '*.cpp'
}

if [ "${1:-}" = "--check" ]; then
    list_sources | xargs -0 "$clang_format" --dry-run --Werror

    # Sources are plain ASCII; other characters are written as escapes (see docs/CODE_STYLE.md).
    if list_sources | LC_ALL=C xargs -0 grep -nP '[^\x00-\x7F]'; then
        echo "The lines above contain characters outside ASCII. Write them as escapes." >&2
        exit 1
    fi
    echo "All files are formatted."
else
    list_sources | xargs -0 "$clang_format" -i
fi
