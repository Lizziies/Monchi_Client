import os
import pathlib
import subprocess
import tempfile
import unittest

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / 'check_publish.py'


class Publication(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.env = os.environ.copy()
        self.env['LOCALAPPDATA'] = str(self.root / 'local')
        words = self.root / 'local/Monchi/private-words.txt'
        words.parent.mkdir(parents=True)
        words.write_text('private-identity', encoding='utf8')
        self.repo = self.root / 'repo'
        self.repo.mkdir()
        self.git('init', '-q')
        self.git('config', 'user.name', 'Monchi')
        self.git('config', 'user.email', 'monchi@users.noreply.github.com')

    def git(self, *args):
        subprocess.run(['git', *args], cwd=self.repo, check=True, capture_output=True)

    def snapshot(self, name='source.txt', data=b'clean source'):
        path = self.repo / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        self.git('add', '-A')
        self.git('commit', '-qm', 'initial source')

    def check(self, cwd=None):
        return subprocess.run(['python', str(SCRIPT), '--require-private-words'], cwd=cwd or self.repo, env=self.env, capture_output=True, text=True).returncode

    def test_clean_snapshot(self):
        self.snapshot()
        self.assertEqual(self.check(), 0)

    def test_vendor_and_utf16_are_checked(self):
        self.snapshot('vendor/example/file.txt', 'private-identity'.encode('utf-16le'))
        self.assertEqual(self.check(), 1)

    def test_private_artifact(self):
        self.snapshot('online.key', b'test fixture')
        self.assertEqual(self.check(), 1)

    def test_not_a_repository(self):
        self.assertEqual(self.check(self.root), 1)

    def test_dirty_snapshot(self):
        self.snapshot()
        (self.repo / 'source.txt').write_text('changed')
        self.assertEqual(self.check(), 1)


if __name__ == '__main__':
    unittest.main()
