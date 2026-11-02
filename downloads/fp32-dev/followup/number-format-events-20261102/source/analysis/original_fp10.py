"""Locate and verify the exact user-supplied Flash Player 10 projector."""
from pathlib import Path
import hashlib
import os

SHA256='c72b5cf816033bb63ab56a5508f372b11672379d5fe537d9aef7cc4cd00565ed'
def original_player():
    explicit=os.environ.get('FP10_ORIGINAL')
    candidates=[Path(explicit)] if explicit else list(Path('/workspace/attachments').glob('*/flashplayer10_3r183_90_win_sa_debug.exe'))
    for path in candidates:
        if path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest()==SHA256:
            return path
    raise SystemExit('The exact Flash Player 10.3.183.90 debug projector is required. Set FP10_ORIGINAL to the supplied file; expected SHA-256 '+SHA256)
