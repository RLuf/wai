#!/usr/bin/env python3
"""
Guardrail Hunter Script

Scans the Gemma source for common behavioral filters and alignment hooks,
reporting occurrences of tokens softcaps, AcceptFunc/SampleFunc, and related
patterns to help identify embedded guardrails.
"""
import os
import re
import sys


PATTERNS = [
    ("Attention Softcap (MaybeLogitsSoftCap)", re.compile(r"MaybeLogitsSoftCap")),
    ("Final Softcap (config.final_cap)", re.compile(r"Softcap\s*\(") ),
    ("Backward Softcap Grad (SoftcapVJP)", re.compile(r"SoftcapVJP")),
    ("Backward Softcap Grad T (SoftcapVJPT)", re.compile(r"SoftcapVJPT")),
    ("AcceptFunc hook", re.compile(r"AcceptFunc")),
    ("SampleFunc hook", re.compile(r"SampleFunc")),
    ("Attention cap param (att_cap)", re.compile(r"\batt_cap\b")),
    ("Final cap param (final_cap)", re.compile(r"\bfinal_cap\b")),
    ("Weights config references", re.compile(r"weights_config")),
]


def hunt(root_dir="gemma_src"):
    for name, pattern in PATTERNS:
        print(f"=== {name} ===")
        for dirpath, _, files in os.walk(root_dir):
            for fname in files:
                if not fname.endswith((".h", ".cc", ".cpp", ".c", ".py")):
                    continue
                path = os.path.join(dirpath, fname)
                try:
                    with open(path, encoding="utf-8", errors="ignore") as f:
                        for idx, line in enumerate(f, start=1):
                            if pattern.search(line):
                                print(f"{path}:{idx}: {line.strip()}")
                except OSError:
                    continue
        print()


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "gemma_src"
    if not os.path.isdir(root):
        print(f"Error: directory '{root}' not found", file=sys.stderr)
        sys.exit(1)
    hunt(root)


if __name__ == "__main__":
    main()
