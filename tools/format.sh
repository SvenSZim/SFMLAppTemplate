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

if [ "${1:-}" = "--check" ]; then
    git ls-files -z '*.hpp' '*.cpp' | xargs -0 "$clang_format" --dry-run --Werror
    echo "All files are formatted."
else
    git ls-files -z '*.hpp' '*.cpp' | xargs -0 "$clang_format" -i
fi
