#!/usr/bin/env sh
# commit-msg: prefixes the subject with "[<branch>] ". Runs after the editor, so an empty message
# still aborts. Skips detached HEAD (rebase), merges, and subjects that already carry the prefix.
set -eu

message=$1
branch=$(git symbolic-ref --short -q HEAD) || exit 0
git rev-parse -q --verify MERGE_HEAD >/dev/null && exit 0

prefix="[$branch] "

awk -v prefix="$prefix" '
    !done && $0 !~ /^#/ && $0 ~ /[^[:space:]]/ {
        if (index($0, prefix) != 1)
            $0 = prefix $0
        done = 1
    }
    { print }
' "$message" > "$message.tmp"

mv "$message.tmp" "$message"
