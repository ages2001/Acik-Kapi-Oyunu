"""Send physical QEMU keyboard events and capture a bounded gameplay segment.

Example: game_input.py tutorial-run --repeat 8 --hold 2.5 --release .5 right up x
The first key remains held; the others are released between repetitions.
This exercises the untouched game and never reads or changes game variables.
"""
import argparse
import json
from pathlib import Path
import socket
import time
from PIL import Image

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('name')
p.add_argument('--repeat', type=int, default=1)
p.add_argument('--hold', type=float, default=3)
p.add_argument('--release', type=float, default=.5)
p.add_argument('--pulse-all', action='store_true')
p.add_argument('--pause-after', action='store_true')
p.add_argument('--resume-before', action='store_true')
p.add_argument('--resume-delay', type=float, default=3,
               help='Delay after releasing P; use 0 for already airborne movement')
p.add_argument('--suspend-vm', action='store_true',
               help='Resume QEMU before input and stop it afterwards, avoiding repeated game pause animations')
p.add_argument('--carry-direction', action='store_true',
               help='Keep the first left/right key held across stopped-VM calls; avoid accidental game double-taps')
p.add_argument('keys', nargs='*', help='Omit keys for an input-free observation segment')
a = p.parse_args()
assert 0 < a.repeat <= 20 and 0 < a.hold <= 20 and 0 <= a.release <= 5
assert 0 <= a.resume_delay <= 10
assert not (a.suspend_vm and a.pause_after), 'Choose game pause or VM suspension'
assert not a.carry_direction or (a.suspend_vm and not a.pulse_all and a.keys and a.keys[0] in ('left','right'))
root = Path(__file__).resolve().parents[2]
held_path = root/'fp32/captures/.held-direction.json'
previous_direction = json.loads(held_path.read_text()).get('direction') if held_path.exists() else None
assert previous_direction in (None, 'left', 'right')
kept_direction = a.keys[0] if a.carry_direction else None
out = root/'fp32/captures'/a.name
out.mkdir(exist_ok=False)
s = socket.socket(socket.AF_UNIX)
s.settimeout(15)
s.connect(str(root/'fp32/nt31-game-qmp.sock'))
f = s.makefile('rwb', buffering=0)

def receive():
    while True:
        result = json.loads(f.readline())
        if 'event' not in result:
            return result

def call(command, arguments):
    f.write((json.dumps({'execute':command,'arguments':arguments})+'\n').encode())
    result = receive()
    if 'error' in result:
        raise RuntimeError(result)

def key(name, down):
    call('input-send-event', {'events':[{'type':'key','data':{
        'down':down,'key':{'type':'qcode','data':name}}}]})

receive()
call('qmp_capabilities', {})
start = time.monotonic()
rows = []
completed = False
try:
    if a.suspend_vm:
        call('cont', {})
    # A paused VM can retain a physically held direction from the previous call.
    # Repeated release/press cycles trigger SM63's deliberate double-tap run.
    if previous_direction and previous_direction != kept_direction:
        key(previous_direction, False)
    if a.resume_before:
        key('p', True)
        time.sleep(.8)
        key('p', False)
        # SM63 animates its pause panel before restoring gameplay controls.
        # At the current 5–6 FPS, a one-second delay loses initial key presses.
        time.sleep(a.resume_delay)
    if a.keys and not a.pulse_all and not (a.carry_direction and previous_direction == kept_direction):
        key(a.keys[0], True)
    pulsed=a.keys if a.pulse_all else a.keys[1:]
    for i in range(a.repeat):
        for k in pulsed:
            key(k, True)
        time.sleep(a.hold)
        for k in pulsed:
            key(k, False)
        time.sleep(a.release)
        ppm = out/f'{i:02d}.ppm'
        # Screenshot conversion can take longer than the intended key pulse.
        # Freeze the guest during capture so held movement does not drift.
        if a.suspend_vm:
            call('stop', {})
        call('screendump', {'filename':str(ppm)})
        Image.open(ppm).save(ppm.with_suffix('.png'))
        ppm.unlink()
        rows.append({'elapsed_seconds':time.monotonic()-start,'screenshot':f'{i:02d}.png'})
        if a.suspend_vm:
            call('cont', {})
    completed = True
finally:
    for k in a.keys:
        if not (completed and k == kept_direction):
            key(k, False)
    if not completed and previous_direction:
        key(previous_direction, False)
    held_path.write_text(json.dumps({'direction':kept_direction if completed else None})+'\n')
    if a.pause_after:
        key('p', True)
        time.sleep(.8)
        key('p', False)
    if a.suspend_vm:
        call('stop', {})
    (out/'input.json').write_text(json.dumps({'arguments':vars(a),'previous_direction':previous_direction,
        'kept_direction':kept_direction if completed else None,'frames':rows},indent=2)+'\n')
    f.close()
    s.close()
print(out)
