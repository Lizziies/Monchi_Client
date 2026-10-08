#!/usr/bin/env python3
"""Export tracked files without repository history or local build state."""
import argparse
import pathlib
import shutil
import subprocess

# working notes stay in the private checkout: of the top-level documents only these are published
EXCLUDE = ('docs/', 'cosmetics/line/previews/', 'cosmetics/line/HANDOFF', 'cosmetics/line/COSMETIC_')
PUBLIC_DOCS = {'README.md', 'SECURITY.md', 'LICENSE', 'LICENSE.md', 'CHANGELOG.md', 'CONTRIBUTING.md'}
FILES = {'vendor/flarial/src/Assets/Assets.aps'}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('destination', type=pathlib.Path)
    args = parser.parse_args()
    source = pathlib.Path(__file__).resolve().parents[1]
    target = args.destination.resolve()
    if target == source or source in target.parents:
        raise RuntimeError('export must be outside the source checkout')
    if not (target / '.git').exists():
        raise RuntimeError('destination must be an empty Git checkout')
    existing = [p for p in target.iterdir() if p.name != '.git']
    if existing:
        raise RuntimeError('destination already contains files')
    names = subprocess.check_output(['git', 'ls-files', '-z'], cwd=source).decode('utf8').split('\0')
    copied = 0
    for name in filter(None, names):
        notes = '/' not in name and name.endswith('.md') and name not in PUBLIC_DOCS
        # hidden folders of local tools; .github holds the workflows and stays
        tool = name.startswith('.') and '/' in name and not name.startswith('.github/')
        if name.startswith(EXCLUDE) or name in FILES or notes or tool:
            continue
        original = source / name
        if original.is_symlink() or not original.is_file():
            raise RuntimeError('export contains a symlink or missing file')
        destination = (target / name).resolve()
        if target not in destination.parents:
            raise RuntimeError('export path escapes destination')
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(original, destination)
        copied += 1
    print(f'exported {copied} tracked files; no history, screenshots or local configuration copied')


if __name__ == '__main__':
    main()
