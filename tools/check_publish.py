#!/usr/bin/env python3
"""Checks that what is about to be published holds nothing personal. Run it in the tree that will be uploaded.

Exit code 0 means nothing was found. The owner's private words (gamertag, names, addresses) are read from
%LOCALAPPDATA%/Monchi/private-words.txt, one per line; that file is never part of a repository.
"""
import os
import pathlib
import re
import subprocess
import sys

SKIP = ('vendor/', 'dll/lib/', 'launcher/lib/')
PATTERNS = [
    ('mail address', re.compile(rb'[A-Za-z0-9._%+-]+@(?!users\.noreply\.github\.com|example\.)[A-Za-z0-9-]+\.(?:com|de|net|org|io|gg)\b')),
    ('user path', re.compile(rb'[A-Za-z]:[\\/]+Users[\\/]+(?!<user>|Public|Default|%)[A-Za-z0-9._ -]+', re.I)),
    ('github token', re.compile(rb'gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}')),
    ('private key', re.compile(rb'-----BEGIN [A-Z ]*PRIVATE KEY')),
    ('aws key', re.compile(rb'AKIA[0-9A-Z]{16}')),
    ('api key', re.compile(rb'sk-[A-Za-z0-9_-]{24,}')),
]
# working notes stay with the owner: only these top-level documents and no notes folder are published
PUBLIC_DOCS = {'README.md', 'SECURITY.md', 'LICENSE', 'LICENSE.md', 'CHANGELOG.md', 'CONTRIBUTING.md'}
CODE_SUFFIXES = {'.c', '.cpp', '.h', '.hpp', '.lua', '.py', '.mjs', '.js', '.ts', '.cmake'}
DATED_COMMENT = re.compile(rb'(//|#|--).*20[0-9]{2}-[01][0-9]-[0-3][0-9]')
PRIVATE_SUFFIXES = {'.exe', '.dll', '.pdb', '.dmp', '.mdmp', '.log', '.key', '.pem', '.sqlite', '.sqlite3', '.db', '.zip', '.7z', '.aps'}


def git(*args):
    return subprocess.run(['git', *args], check=True, capture_output=True, text=True, encoding='utf8', errors='replace').stdout


def private_words():
    path = os.path.join(os.environ.get('LOCALAPPDATA', ''), 'Monchi', 'private-words.txt')
    if not os.path.isfile(path):
        if '--require-private-words' in sys.argv:
            raise RuntimeError('private word list is missing; refusing publication check')
        print('no local private word list: only the general patterns are checked')
        return []
    with open(path, encoding='utf8') as f:
        return [w.strip().lower().encode('utf8') for w in f if len(w.strip()) >= 3]


def main():
    words = private_words()
    found = 0
    for name in filter(None, git('ls-files', '-z').split('\0')):
        if not os.path.isfile(name):
            print(f'{name}: tracked file missing')
            found += 1
            continue
        path = pathlib.Path(name)
        if path.suffix.lower() in PRIVATE_SUFFIXES or path.name in {'local.cmake', 'private-words.txt', '.env'} or path.name.startswith('.env.'):
            print(f'{name}: local or private artifact is tracked')
            found += 1
        if ('/' not in name and path.suffix.lower() == '.md' and name not in PUBLIC_DOCS) or name.startswith('docs/') or 'handoff' in name.lower():
            print(f'{name}: working notes or instructions, not part of a publication')
            found += 1
        with open(name, 'rb') as f:
            data = f.read()
        if path.suffix.lower() in CODE_SUFFIXES and not name.startswith(SKIP) and DATED_COMMENT.search(data):
            print(f'{name}: a dated note in a comment')
            found += 1
        low = data.lower()
        path_low = name.lower().encode('utf8')
        for word in words:
            wide = word.decode('utf8').encode('utf-16le').lower()
            if word in low or word in path_low or wide in low:
                print(f'{name}: a private word of the owner')
                found += 1
        for what, pattern in PATTERNS:
            if what == 'mail address' and name.startswith(SKIP):
                continue
            m = pattern.search(data)
            if m:
                print(f'{name}: {what}')
                found += 1
    people = set(git('log', '--format=%an <%ae>%n%cn <%ce>').splitlines())
    for person in sorted(people):
        low = person.lower().encode('utf8')
        if 'noreply' not in person or any(w in low for w in words):
            print('history: author or committer with a personal address or name')
            found += 1
    if len(git('rev-list', '--all').splitlines()) > 1 and '--allow-history' not in sys.argv:
        print('history: more than one commit; the public repository starts with a single fresh commit')
        found += 1
    messages = git('log', '--format=%B').encode('utf8').lower()
    if any(w in messages for w in words) or any(pattern.search(messages) for _, pattern in PATTERNS):
        print('history: private content in a commit message')
        found += 1
    if git('diff', '--name-only', 'HEAD').strip():
        print('tracked files differ from the commit; commit and check the exact publication snapshot')
        found += 1
    print('nothing personal found' if not found else f'{found} findings: do not publish')
    return 1 if found else 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f'publication check failed: {type(error).__name__}')
        sys.exit(1)
