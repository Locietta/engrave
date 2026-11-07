"""Selected excerpts: real writer captures and independent 4.7.6 semantics.

Run capture on either host; --verify-only allows Windows artifacts to be
transferred to the physical Linux oracle without rerunning the writer.
"""
import sys

sys.path.insert(0, str(__import__("pathlib").Path(__file__).resolve().parents[2] / "tests"))
from support.paths import INPUT_ROOT, REPO_ROOT

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET
import zipfile


def xml(path):
    if path.suffix == '.mscz':
        with zipfile.ZipFile(path) as archive:
            return ET.fromstring(archive.read('score.mscx'))
    return ET.parse(path).getroot()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--probe', type=Path)
    ap.add_argument('--assets', type=Path)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--oracle', type=Path)
    ap.add_argument('--verify-only', action='store_true')
    args = ap.parse_args()
    fixtures = {'excerpts': REPO_ROOT / 'engrave/tests/data/write470/excerpts.mscx',
                'linked_parts': INPUT_ROOT / 'golden/linked_parts.mscx'}
    if not args.verify_only:
        args.out.mkdir(parents=True, exist_ok=False)
        for name, source in fixtures.items():
            for fmt in ('mscx', 'mscz'):
                for version in ('4.70', 'default', '5.00'):
                    output = args.out / f'{name}-{version}.{fmt}'
                    run = subprocess.run([str(args.probe), str(args.assets), str(source), str(output), fmt, version, 'excerpt'], capture_output=True)
                    output.with_suffix(output.suffix + '.stderr').write_bytes(run.stderr)
                    assert run.returncode == 0, run.stderr.decode(errors='replace')
                    warnings = json.loads(run.stdout)
                    output.with_suffix(output.suffix + '.warnings.json').write_text(json.dumps(warnings))
                    assert warnings == [], warnings
                    root = xml(output)
                    assert root.get('version') == ('4.70' if version == '4.70' else '5.00')
                    assert root.find('Score/Score') is None
            # Every source excerpt EID and every link emitted by standalone 5.00
            # must survive the downgrade (part EIDs are the documented exception).
            current = xml(args.out / f'{name}-5.00.mscx')
            legacy = xml(args.out / f'{name}-4.70.mscx')
            def identities(root):
                return {e.findtext('eid'): (e.tag, [x.text for x in e.findall('linkedTo')])
                        for e in root.iter() if e.find('eid') is not None and e.tag != 'Part'}
            assert identities(current) == identities(legacy)
        print('12 successful exports; exact empty warning arrays; EIDs/links retained (5.00 bytes checked in-process by Export470)')
    if args.oracle:
        oracle = args.oracle.resolve()
        checks = (oracle / 'SHA256SUMS').read_text().splitlines()
        assert len(checks) == 87
        for line in checks:
            digest, filename = line.split(maxsplit=1)
            assert hashlib.sha256((oracle / filename.lstrip('*')).read_bytes()).hexdigest() == digest
        env = {'PATH': '/usr/bin:/bin', 'LD_LIBRARY_PATH': str(oracle / 'lib')}
        def dump(path, label):
            run = subprocess.run([str(oracle / 'bin/semantic_dump'), str(oracle / 'assets'), str(path)], env=env, capture_output=True)
            (args.out / (label + '.oracle.json')).write_bytes(run.stdout)
            (args.out / (label + '.oracle.stderr')).write_bytes(run.stderr)
            assert run.returncode == 0 and not run.stderr, run.stderr
            return json.loads(run.stdout)
        for name, source in fixtures.items():
            expected = dump(source, name + '-source')['excerpts'][0]['score']
            for fmt in ('mscx', 'mscz'):
                output = args.out / f'{name}-4.70.{fmt}'
                assert json.loads(output.with_suffix(output.suffix + '.warnings.json').read_text()) == []
                actual = dump(output, output.name)
                assert actual['excerpts'] == []
                for key, value in expected.items():
                    assert actual[key] == value, (name, fmt, key)
        print('87 oracle checksums verified; four complete excerpt bodies match independent source dumps; isolated lib/')


if __name__ == '__main__':
    main()
