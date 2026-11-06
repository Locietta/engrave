"""Install the fork's staged format checker."""
from pathlib import Path
import shutil
import subprocess
root = Path(__file__).resolve().parents[1]
hooks = Path(subprocess.check_output(["git", "-C", str(root), "rev-parse", "--path-format=absolute", "--git-path", "hooks"], text=True).strip())
hooks.mkdir(parents=True, exist_ok=True)
target = hooks / "pre-commit"
if target.is_symlink():
    target.unlink()
shutil.copyfile(root / "tools/pre-commit", target)
target.chmod(0o755)
