#!/usr/bin/env python3
"""Header guards are DOOM_OS_ plus the repo-relative path. Wrong names are rewritten in place."""

import re
import sys

GUARD = re.compile(r"^#ifndef[ \t]+(\w+)[ \t]*\n#define[ \t]+(\w+)[ \t]*$", re.M)
ENDIF = re.compile(r"^#endif[ \t]+(?://[ \t]*(\w+)|/\*[ \t]*(\w+)[ \t]*\*/)[ \t]*$")


def expected(path):
    return "DOOM_OS_" + re.sub(r"[^A-Za-z0-9]", "_", path).upper() + "_"


def check(path):
    with open(path) as stream:
        text = stream.read()

    want = expected(path)
    guard = GUARD.search(text)

    if guard is None:
        return f"{path}: no '#ifndef {want}' followed by '#define {want}'"
    if guard[1] != guard[2]:
        return f"{path}: #ifndef {guard[1]} but #define {guard[2]}"

    last = text.rstrip().rsplit("\n", 1)[-1]
    endif = ENDIF.match(last)

    if endif is None or guard[1] not in endif.groups():
        return f"{path}: last line must be '#endif  // {guard[1]}'"
    if guard[1] == want:
        return None

    with open(path, "w") as stream:
        stream.write(re.sub(rf"\b{guard[1]}\b", want, text))
    return f"{path}: renamed {guard[1]} -> {want}"


def main():
    problems = [message for path in sys.argv[1:] if (message := check(path))]
    for message in problems:
        print(message)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
