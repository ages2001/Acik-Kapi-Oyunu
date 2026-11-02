"""Locate and verify the exact user-supplied original player before analysis."""
from pathlib import Path
import hashlib
import os

SHA256 = 'a4b333ac1da12026989549015303d82231982838bccfb544ba5fd188746066f0'

def original_player():
    root = Path(__file__).resolve().parents[2]
    configured = os.environ.get('FP32_ORIGINAL')
    candidates = ([Path(configured)] if configured else [
        root/'fp32/original/FLASH32.EXE',
        Path('/workspace/attachments/e9603734-a869-44cc-91b8-17bc6bd775d5/flashplayer32_0r0_465_win_sa.exe'),
    ])
    for path in candidates:
        if path.is_file():
            actual = hashlib.sha256(path.read_bytes()).hexdigest()
            if actual != SHA256:
                raise ValueError(f'Unexpected original player SHA-256 at {path}: {actual}')
            return path
    raise FileNotFoundError('Set FP32_ORIGINAL to the supplied Flash Player 32.0.0.465 EXE.')
