"""Apply the released native adaptations, then Melee Party's, to the pinned public Melee submodule."""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = '039c4bf4ca33338c35d21901ad19b7ede19d19ad'
NATIVE = ROOT / 'sourceport/patches/melee-native.patch'
# Melee Party's hooks (sourceport/game/party), on top of the native adaptations.
PARTY = ROOT / 'sourceport/patches/melee-party.patch'


def prepare(decomp):
    def git(*args):
        return subprocess.run(['git', '-C', str(decomp), *args], capture_output=True, text=True)

    def apply(patch, what):
        check = git('apply', '--check', str(patch))
        if check.returncode:
            raise SystemExit(check.stderr)
        result = git('apply', str(patch))
        if result.returncode:
            raise SystemExit(result.stderr)
        print(f'Prepared the {what}.')

    # The party hunks sit beside native ones and change their context, so once both are applied only
    # the party patch reverses cleanly; it applies only on top of the native one.
    if PARTY.exists() and git('apply', '--reverse', '--check', str(PARTY)).returncode == 0:
        print('Native Melee sources and Melee Party hooks are already prepared.')
        return
    if git('apply', '--reverse', '--check', str(NATIVE)).returncode == 0:
        print('Native Melee sources are already prepared.')
    else:
        head = git('rev-parse', 'HEAD')
        if head.returncode or head.stdout.strip() != BASE:
            raise SystemExit('Initialize the pinned sources first: git submodule update --init sourceport/extern/melee')
        status = git('status', '--porcelain', '--untracked-files=no')
        if status.returncode or status.stdout.strip():
            raise SystemExit('Melee sources have local changes. Use a clean checkout before applying the native patch.')
        apply(NATIVE, 'released native Melee sources')

    if PARTY.exists():
        if git('apply', '--reverse', '--check', str(PARTY)).returncode == 0:
            print('Melee Party hooks are already applied.')
        else:
            apply(PARTY, 'Melee Party hooks')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--decomp', type=Path, default=ROOT / 'sourceport/extern/melee')
    prepare(parser.parse_args().decomp)
