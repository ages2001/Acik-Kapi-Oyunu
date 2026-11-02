"""Interactive, sequential local QEMU test controls (JSON lines, no shell).

Keep one tool-owned session for repeat test input/captures. This avoids many
short-lived control processes while retaining an auditable action transcript.
Never submit an action for a guest whose QMP is in use by another test.
"""
from pathlib import Path
import json,re,subprocess,sys
here=Path(__file__).resolve().parent
allowed={'query-status','device_add','device_del','qom-get','qom-set','set_link','quit'}
print('READY: local QEMU test controls; one JSON action per line',flush=True)
for line in sys.stdin:
    try:
        action=json.loads(line);guest=action['guest'];op=action['op']
        assert re.fullmatch(r'[a-z0-9-]+',guest)
        assert (Path('/workspace/rltklant-vm/run')/guest/'launch.json').exists()
        if op in ('key','type'):
            args=['type_guest.py',guest,op,action['value']]
        elif op=='capture':
            stem=action['value'];assert re.fullmatch(r'[A-Za-z0-9_-]+',stem)
            assert not (here/(stem+'.png')).exists(),'preserve previous capture'
            args=['capture_guest.py',guest,stem]
        elif op=='qmp':
            assert action['command'] in allowed
            args=['qmp.py',guest,action['command'],json.dumps(action.get('arguments',{}))]
        else:raise ValueError('unsupported operation')
        result=subprocess.run([sys.executable,str(here/args[0]),*args[1:]],check=False,timeout=90)
        print(json.dumps({'done':op,'guest':guest,'exit_code':result.returncode}),flush=True)
    except Exception as e:
        print(json.dumps({'error':str(e)}),flush=True)
