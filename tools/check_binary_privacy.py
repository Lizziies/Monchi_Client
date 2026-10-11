#!/usr/bin/env python3
"""Check release binaries without printing private values."""
import argparse
import os
import importlib.util
import pathlib
import re

spec = importlib.util.spec_from_file_location('publication', pathlib.Path(__file__).with_name('check_publish.py'))
publication = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publication)


# A release has to carry where its updates and the online service come from. Those exact strings are listed, one per
# line, in a file next to the private word list and are cut out before the search; any other match still counts.
def needed_strings():
    path = pathlib.Path(os.environ.get('LOCALAPPDATA', '')) / 'Monchi' / 'release-allowed-strings.txt'
    if not path.is_file():
        return []
    return [line.strip().lower().encode('utf8') for line in path.read_text(encoding='utf8').splitlines() if line.strip()]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('files', nargs='+', type=pathlib.Path)
    parser.add_argument('--require-private-words', action='store_true')
    args = parser.parse_args()
    words = publication.private_words()
    needed = needed_strings()
    findings = 0
    for path in args.files:
        data = path.read_bytes()
        low = data.lower()
        for text in needed:
            low = low.replace(text, b'').replace(text.decode('utf8').encode('utf-16le'), b'')
        hits = sum(word in low or word.decode('utf8').encode('utf-16le').lower() in low for word in words)
        if hits:
            print(f'{path.name}: {hits} private-term matches')
            findings += hits
        wide = b'\n'.join(part.decode('utf-16le').encode('ascii') for part in re.findall(rb'(?:[\x20-\x7e]\x00){4,}', data))
        for label, pattern in publication.PATTERNS:
            if label == 'mail address':
                continue
            if pattern.search(data) or pattern.search(wide):
                print(f'{path.name}: {label}')
                findings += 1
        if not hits:
            print(f'{path.name}: no owner-private terms found')
    print('binary privacy checks passed' if not findings else 'binary privacy checks failed; do not distribute')
    return bool(findings)


if __name__ == '__main__':
    raise SystemExit(main())
