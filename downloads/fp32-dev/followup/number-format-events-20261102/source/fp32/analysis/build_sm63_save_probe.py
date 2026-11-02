"""Build an isolated fixture from SM63's exported AS2 persistence functions.

The original SWF and its storage are never modified. Only the SharedObject name
in the copied functions changes. This is a serialization test, not gameplay.
"""
from pathlib import Path
import hashlib
import json
import subprocess

root = Path(__file__).resolve().parents[2]
scripts = root / 'fp32/analysis/sm63-scripts/scripts'
source = (scripts / 'frame_2/DoAction.as').read_text()

def function(name):
    start = source.index('_root.' + name + ' = function(')
    end = source.index('\n};', start) + 3
    return source[start:end]

names = ['SaveFile', 'LoadFile', 'CalculateStars', 'CalculateStarCoins', 'CalculateFilesInfo']
parts = [(scripts / 'frame_1/DoAction_2.as').read_text()]
parts.extend(function(name) for name in names)
original = '\n'.join(parts)
copied = original.replace('SuperMarioSunshine128SavedFile', 'Ages2001SM63SerializationProbeV1')
harness = r'''
stop();
_root.createTextField("status",1000,8,8,435,285);
_root.status.background = true;
_root.status.backgroundColor = 16777215;
_root.status.textColor = 0;
_root.File = "A";
_root.probe = SharedObject.getLocal("Ages2001SM63SerializationProbeV1");
_root.phase = "Fresh process read";
_root.check = function(ok) {
 _root.checks++;
 if (ok !== true) { _root.failures++; }
};
_root.verify = function() {
 _root.checks=0; _root.failures=0;
 _root.LoadFile();
 _root.check(_root.savedfiles.data.FileExists === true);
 _root.check(_root.Star[39] === true);
 _root.check(_root.Star[1] === false);
 _root.check(_root.Star[64] === false);
 _root.check(_root.TotalStars === 1);
 _root.check(_root.StarCoin[1] === true);
 _root.check(_root.StarCoin[64] === false);
 _root.check(_root.TotalStarCoins === 1);
 _root.check(_root.FluddArray[1][1] === true);
 _root.check(_root.FluddArray[1][2] === false);
 _root.check(_root.FluddArray[11][3] === true);
 _root.check(_root.DoorOpenedArray[3] === true);
 _root.check(_root.DoorOpenedArray[13] === false);
 _root.check(_root.BowserKey1 === false);
 _root.check(_root.BowserKey2 === true);
 _root.check(_root.BowserKey3 === false);
 _root.check(_root.FileBStar[2] === true);
 _root.check(_root.FileCStar[3] === true);
 _root.check(_root.FileDStar[64] === true);
 _root.check(_root.FileBTotalStars === 1);
 _root.check(_root.FileCTotalStars === 1);
 _root.check(_root.FileDTotalStars === 1);
 _root.check(_root.RecordCoin1 === 87);
 _root.check(_root.RecordCoin11 === 99);
 _root.Star[39] = false;
 _root.FluddArray[1][1] = false;
 _root.check(_root.savedfiles.data.FileAStar[39] === true);
 _root.check(_root.savedfiles.data.FileAFludd[1][1] === true);
 _root.status.text = "SM63 AS2 serialization fixture\n" + _root.phase +
  "\nChecks: " + _root.checks + " / failures: " + _root.failures +
  "\nOriginal SaveFile / LoadFile / deep copy code\n4 slots; stars, coins, FLUDD, keys, doors\n" +
  "Independent fixture storage; synthetic progress.\nNot a real SM63 gameplay save/reopen pass.\nPress S to write; reopen to verify from disk.";
};
_root.writeProbe = function() {
 var letters = ["A","B","C","D"];
 var slot; var j; var data = _root.probe.data;
 for (slot=0; slot<4; slot++) {
  var prefix="File"+letters[slot];
  data[prefix+"Star"]=[""]; data[prefix+"StarCoin"]=[""];
  for (j=1;j<=64;j++) {data[prefix+"Star"][j]=false; data[prefix+"StarCoin"][j]=false;}
  data[prefix+"Star"][slot==3 ? 64 : slot+1]=true;
  data[prefix+"Fludd"]=[""];
  for (j=1;j<=11;j++) {data[prefix+"Fludd"][j]=["",false,false,false];}
  data[prefix+"DoorOpenedArray"]=[];
  for (j=0;j<14;j++) {data[prefix+"DoorOpenedArray"][j]=false;}
  data[prefix+"BowserKey1"]=false; data[prefix+"BowserKey2"]=false; data[prefix+"BowserKey3"]=false;
 }
 data.RecordCoin1=87; data.RecordCoin11=99;
 _root.LoadFile();
 _root.Star[1]=false; _root.Star[39]=true;
 _root.StarCoin[1]=true;
 _root.FluddArray[1][1]=true; _root.FluddArray[11][3]=true;
 _root.DoorOpenedArray[3]=true; _root.BowserKey2=true;
 _root.SaveFile();
 data.fixtureVersion=1;
 _root.phase="Write flush: " + _root.probe.flush();
 _root.verify();
};
_root.listener=new Object();
_root.listener.onKeyDown=function() {if(Key.getCode()==83) {_root.writeProbe();}};
Key.addListener(_root.listener);
if (_root.probe.data.fixtureVersion === 1) {_root.verify();}
else {_root.status.text="SM63 AS2 serialization fixture\nNo previous fixture data.\nPress S to write synthetic test values.\nThe real game's save is not modified.";}
'''
out = root / 'fp32/fixtures/sm63-save-as/frame_1'
out.mkdir(parents=True, exist_ok=True)
(out / 'DoAction.as').write_text(copied + '\n' + harness)
subprocess.run(['java', '-jar', str(root/'tools/ffdec/ffdec-cli.jar'),
                '-importScript', str(root/'fp32/build/SAVETEST.SWF'),
                str(root/'fp32/build/SM63SAVE.SWF'), str(out.parent)], check=True)
report = {'copied_functions': names + ['deepObjectCopy'],
          'original_functions_sha256': hashlib.sha256(original.encode()).hexdigest(),
          'only_copied_function_change': 'Isolated SharedObject name',
          'fixture_sha256': hashlib.sha256((root/'fp32/build/SM63SAVE.SWF').read_bytes()).hexdigest(),
          'scope': 'Synthetic serialization fixture; not real gameplay progress.'}
(root/'fp32/reports/sm63-save-fixture-build.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
