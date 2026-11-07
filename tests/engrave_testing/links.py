import os
import shutil
import sys
from pathlib import Path


def setup_link(target: Path, link: Path) -> None:
    target = target.resolve()
    link.parent.mkdir(parents=True, exist_ok=True)
    if sys.platform == "win32":
        import _winapi

        if link.is_symlink() or (hasattr(link, "is_junction") and link.is_junction()) or link.exists():
            try:
                if link.resolve() == target:
                    return
            except OSError:
                pass
            if hasattr(link, "is_junction") and link.is_junction():
                os.rmdir(link)
            elif link.is_symlink() or link.is_file():
                link.unlink()
            elif link.is_dir():
                shutil.rmtree(link)
        try:
            _winapi.CreateJunction(str(target), str(link))
        except Exception:
            os.symlink(str(target), str(link), target_is_directory=True)
    else:
        if link.is_symlink() or link.exists():
            try:
                if link.resolve() == target:
                    return
            except OSError:
                pass
            if link.is_symlink() or link.is_file():
                link.unlink()
            elif link.is_dir():
                shutil.rmtree(link)
        os.symlink(str(target), str(link), target_is_directory=True)
