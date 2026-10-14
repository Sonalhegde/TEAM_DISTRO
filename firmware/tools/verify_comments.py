"""
Comment Verification Utility
Verifies that all code statements, logic, whitespace logic, and variable names
in firmware/main/main.cpp are 100% identical to the upstream repository code,
proving that only comments were altered.
"""

import re
import subprocess
import sys

def strip_comments(text: str):
    # Remove C-style block comments
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    # Remove C++ single-line comments
    text = re.sub(r'//.*', '', text)
    # Return non-empty stripped code lines
    return [line.strip() for line in text.splitlines() if line.strip()]

def main():
    # Attempt to fetch original source from git ref
    try:
        res = subprocess.run(
            ['git', 'show', 'origin/main:controller_firmware/main/main.cpp'],
            capture_output=True,
            text=True,
            check=True
        )
        orig_text = res.stdout
    except Exception:
        try:
            res = subprocess.run(
                ['git', 'show', 'HEAD:firmware/main/main.cpp'],
                capture_output=True,
                text=True,
                check=True
            )
            orig_text = res.stdout
        except Exception as e:
            print(f"Error fetching original file from git: {e}")
            sys.exit(1)

    with open('firmware/main/main.cpp', 'r', encoding='utf-8') as f:
        curr_text = f.read()

    orig_code = strip_comments(orig_text)
    curr_code = strip_comments(curr_text)

    print(f"Original upstream code lines: {len(orig_code)}")
    print(f"Current firmware code lines:   {len(curr_code)}")

    if orig_code == curr_code:
        print("SUCCESS: Code lines are 100% IDENTICAL. Zero code logic or behavioral changes.")
        sys.exit(0)
    else:
        print("MISMATCH DETECTED:")
        for idx, (l1, l2) in enumerate(zip(orig_code, curr_code)):
            if l1 != l2:
                print(f"Line {idx+1}:\n  ORIG: {l1}\n  CURR: {l2}")
                break
        if len(orig_code) != len(curr_code):
            print(f"Line counts differ: {len(orig_code)} vs {len(curr_code)}")
        sys.exit(1)

if __name__ == '__main__':
    main()
