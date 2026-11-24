"""Unit tests for tools/prune_bazel_cache.py."""

import importlib.util
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

TOOL_PATH = Path(__file__).resolve().parent / "prune_bazel_cache.py"
SPEC = importlib.util.spec_from_file_location("prune_bazel_cache_under_test", TOOL_PATH)
assert SPEC and SPEC.loader
prune_tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(prune_tool)


class PruneBazelCacheTests(unittest.TestCase):
    def test_evicts_oldest_entries_and_cleans_tmp(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            (cache / "ac/00").mkdir(parents=True)
            (cache / "cas/aa").mkdir(parents=True)
            (cache / "tmp/partial").mkdir(parents=True)

            (cache / "tmp/orphan").write_bytes(b"x" * 100)
            (cache / "tmp/partial/blob").write_bytes(b"y" * 100)

            old_ac = cache / "ac/00/old"
            old_cas = cache / "cas/aa/old"
            new_ac = cache / "ac/00/new"
            new_cas = cache / "cas/aa/new"

            old_ac.write_bytes(b"a" * 300)
            old_cas.write_bytes(b"b" * 400)
            new_ac.write_bytes(b"c" * 200)
            new_cas.write_bytes(b"d" * 300)

            os.utime(old_ac, ns=(1_000_000_000, 1_000_000_000))
            os.utime(old_cas, ns=(2_000_000_000, 2_000_000_000))
            os.utime(new_ac, ns=(3_000_000_000, 3_000_000_000))
            os.utime(new_cas, ns=(4_000_000_000, 4_000_000_000))

            before, after, removed = prune_tool.prune_disk_cache(cache, max_bytes=500)

            self.assertEqual(before, 1200)
            self.assertEqual(after, 500)
            self.assertEqual(removed, 4)  # 2 tmp entries + old_ac + old_cas
            self.assertFalse(old_ac.exists())
            self.assertFalse(old_cas.exists())
            self.assertTrue(new_ac.is_file())
            self.assertTrue(new_cas.is_file())
            self.assertEqual(list((cache / "tmp").iterdir()), [])

    def test_noop_when_within_budget_still_cleans_tmp(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            (cache / "cas/01").mkdir(parents=True)
            (cache / "tmp").mkdir(parents=True)
            blob = cache / "cas/01/blob"
            blob.write_bytes(b"z" * 256)
            (cache / "tmp/stale").write_bytes(b"t" * 64)

            before, after, removed = prune_tool.prune_disk_cache(cache, max_bytes=512)

            self.assertEqual((before, after, removed), (256, 256, 1))
            self.assertTrue(blob.is_file())
            self.assertEqual(list((cache / "tmp").iterdir()), [])

    def test_cli_runs_shutdown_before_prune_and_rejects_nonpositive_limit(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            order: list[str] = []

            def fake_call(cmd, **kwargs):
                self.assertEqual(cmd, ["fake-bazel", "shutdown"])
                order.append("shutdown")
                return 0

            def fake_prune(path, max_bytes):
                self.assertEqual(path, cache)
                self.assertEqual(max_bytes, 2 * prune_tool.MIB)
                order.append("prune")
                return (0, 0, 0)

            with (
                patch.object(prune_tool.subprocess, "call", side_effect=fake_call),
                patch.object(prune_tool, "prune_disk_cache", side_effect=fake_prune),
            ):
                rc = prune_tool.main(
                    ["--disk-cache", str(cache), "--max-size-mb", "2", "--shutdown", "--bazel", "fake-bazel"]
                )
            self.assertEqual(rc, 0)
            self.assertEqual(order, ["shutdown", "prune"])

            with self.assertRaises(SystemExit) as ctx:
                prune_tool.main(["--disk-cache", str(cache), "--max-size-mb", "0"])
            self.assertNotEqual(ctx.exception.code, 0)

    def test_rejects_symlink_root(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / "real_cache"
            (target / "cas/00").mkdir(parents=True)
            victim = target / "cas/00/blob"
            victim.write_bytes(b"a" * 200)
            link = root / "link_cache"
            try:
                link.symlink_to(target, target_is_directory=True)
            except OSError as error:
                self.skipTest(f"symlink creation unavailable: {error}")

            with self.assertRaises(ValueError):
                prune_tool.prune_disk_cache(link, max_bytes=1)
            self.assertTrue(victim.is_file())


if __name__ == "__main__":
    sys.exit(unittest.main())
