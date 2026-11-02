"""Independent IANA zoneinfo oracle, not a reimplementation of the C rules."""
from pathlib import Path
from datetime import datetime,timedelta,timezone
from zoneinfo import ZoneInfo
import json
root=Path(__file__).resolve().parents[1]
zones=['America/New_York','Europe/Berlin','Australia/Sydney','Pacific/Auckland','Asia/Kolkata']
def fields(dt):return [dt.year,dt.month,(dt.weekday()+1)%7,dt.day,dt.hour,dt.minute,dt.second,dt.microsecond//1000]
rows=[]
for index,name in enumerate(zones):
 z=ZoneInfo(name);dates=set()
 # One sample every day in a leap year; exact instants around every transition.
 day=datetime(2024,1,1,0,17,39,123000,tzinfo=timezone.utc)
 for n in range(367):dates.add(day+timedelta(days=n))
 previous=day.astimezone(z).utcoffset()
 for n in range(24*366):
  instant=datetime(2024,1,1,tzinfo=timezone.utc)+timedelta(hours=n)
  offset=instant.astimezone(z).utcoffset()
  if offset!=previous:
   for delta in [-3601,-3600,-1,0,1,3599,3600]:dates.add(instant+timedelta(seconds=delta))
  previous=offset
 for dt in sorted(dates):rows.append({'zone':index,'utc':fields(dt),'local':fields(dt.astimezone(z))})
header='/* Generated using Python zoneinfo / system IANA tzdata. */\nstatic const struct {UINT zone;SYSTEMTIME utc,local;} vectors[]={\n'
for r in rows:header+='{'+str(r['zone'])+',{'+','.join(map(str,r['utc']))+'},{'+','.join(map(str,r['local']))+'}},\n'
header+='};\n'
(root/'fp32/compat/timezone_fixtures.h').write_text(header)
(root/'fp32/fixtures/timezone-vectors.json').write_text(json.dumps({'oracle':'Python zoneinfo, system IANA tzdata','zones':zones,'vectors':rows},indent=2)+'\n')
print(len(rows))
