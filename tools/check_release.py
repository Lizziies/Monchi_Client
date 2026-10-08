#!/usr/bin/env python3
"""Checks a published release the way others get it: downloads the release files and the repository from GitHub and
runs both publication checks on them. Run it after every release and every push to the public repository.

    python tools/check_release.py <owner>/<repo> [tag]

Without a tag the latest release is checked. Exit code 0 means nothing was found.
"""
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile
import urllib.request

TOOLS = pathlib.Path(__file__).resolve().parent


def fetch(url):
    request = urllib.request.Request(url, headers={'User-Agent': 'monchi-release-check', 'Accept': 'application/vnd.github+json'})
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.read()


def run(*args, cwd=None):
    result = subprocess.run([sys.executable, '-I', *map(str, args)], cwd=cwd, capture_output=True, text=True, encoding='utf8', errors='replace')
    print(result.stdout.rstrip())
    if result.stderr.strip():
        print(result.stderr.rstrip())
    return result.returncode == 0


def checksums(folder, manifest):
    ok = True
    listed = {}
    for line in manifest.splitlines()[1:]:
        digest, _, name = line.partition('  ')
        if name:
            listed[name] = digest
    for path in sorted(folder.iterdir()):
        if path.name.startswith('checksums.'):
            continue
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if listed.get(path.name) != digest:
            print(f'{path.name}: not in checksums.txt or a different file than signed')
            ok = False
    return ok


def main():
    if len(sys.argv) < 2 or '/' not in sys.argv[1]:
        print(__doc__)
        return 2
    repo = sys.argv[1]
    tag = sys.argv[2] if len(sys.argv) > 2 else None
    api = f'https://api.github.com/repos/{repo}/releases/' + (f'tags/{tag}' if tag else 'latest')
    release = json.loads(fetch(api))
    print(f'release {release["tag_name"]}, {len(release["assets"])} files')
    ok = True
    with tempfile.TemporaryDirectory() as temp:
        files = pathlib.Path(temp) / 'release'
        files.mkdir()
        for asset in release['assets']:
            (files / asset['name']).write_bytes(fetch(asset['browser_download_url']))
        manifest = files / 'checksums.txt'
        if manifest.is_file():
            ok &= checksums(files, manifest.read_text(encoding='utf8'))
        else:
            print('no checksums.txt in the release')
            ok = False
        binaries = [p for p in sorted(files.iterdir()) if not p.name.startswith('checksums.')]
        ok &= run(TOOLS / 'check_binary_privacy.py', '--require-private-words', *binaries)

        source = pathlib.Path(temp) / 'source'
        clone = subprocess.run(['git', 'clone', '--quiet', '--depth', '1', f'https://github.com/{repo}', str(source)], capture_output=True, text=True)
        if clone.returncode != 0:
            print('the repository could not be cloned')
            ok = False
        else:
            ok &= run(TOOLS / 'check_publish.py', '--require-private-words', cwd=source)
    print('release check passed' if ok else 'release check FAILED: take the files down and fix them')
    return 0 if ok else 1


if __name__ == '__main__':
    raise SystemExit(main())
