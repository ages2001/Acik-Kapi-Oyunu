"""Dismiss only OCR-confirmed Windows 2000 unsafe-removal notices in a test.

Keep a screenshot and OCR transcript for every decision. Never sends input
when the known notification is absent. Caller must own the guest controls.
"""
import json,subprocess,sys
from pathlib import Path
v=Path(__file__).resolve().parent;guest,tag=sys.argv[1:3]
assert guest.startswith('win2000')
rows=[]
for index in range(10):
 stem=guest+'-'+tag+'-notice'+str(index)
 subprocess.run([sys.executable,str(v/'capture_guest.py'),guest,stem],check=True)
 text=subprocess.check_output(['tesseract',str(v/(stem+'.png')),'stdout','--psm','3'],text=True,stderr=subprocess.DEVNULL)
 lower=text.lower(); compact=''.join(c for c in lower if c.isalnum())
 match=all(phrase in compact for phrase in ('youhaveunplugged','devicewithoutstopping',
                                           'showunplugejecticononthetaskbar'))
 rows.append({'screenshot':stem+'.png','ocr':text,'dismissed':match})
 (v/(guest+'-'+tag+'-notices.json')).write_text(json.dumps(rows,indent=2)+'\n')
 if not match:break
 subprocess.run([sys.executable,str(v/'type_guest.py'),guest,'key','ret'],check=True)
else:raise AssertionError('More than ten notices; inspect manually')
print('Dismissed',sum(row['dismissed'] for row in rows),'confirmed unsafe-removal notices')
