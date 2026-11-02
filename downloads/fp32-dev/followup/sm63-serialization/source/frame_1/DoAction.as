function deepObjectCopy(obj)
{
   if(typeof obj != "object" || object instanceof Button || object instanceof TextField || object instanceof MovieClip)
   {
      return obj;
   }
   var _loc3_;
   if(obj instanceof Boolean)
   {
      _loc3_ = new Boolean(Boolean.prototype.valueOf.call(obj));
   }
   else if(obj instanceof Number)
   {
      _loc3_ = new Number(Number.prototype.valueOf.call(obj));
   }
   else if(obj instanceof String)
   {
      _loc3_ = new String(String.prototype.valueOf.call(obj));
   }
   else if(obj.__constructor__)
   {
      if(typeof obj.clone == "function")
      {
         _loc3_ = obj.clone();
         if(_loc3_.__proto__ == obj.__proto__)
         {
            return _loc3_;
         }
      }
      _loc3_ = new obj.__constructor__();
   }
   else if(obj instanceof Array)
   {
      _loc3_ = [];
   }
   else
   {
      _loc3_ = {};
   }
   for(var _loc4_ in obj)
   {
      if(obj.hasOwnProperty(_loc4_))
      {
         _loc3_[_loc4_] = arguments.callee(obj[_loc4_]);
      }
   }
   return _loc3_;
}

_root.SaveFile = function()
{
   trace("File " + _root.File + " Saved!");
   _root.savedfiles = SharedObject.getLocal("Ages2001SM63SerializationProbeV1");
   i = 0;
   while(i < 4)
   {
      if(i == 0)
      {
         file = "A";
      }
      if(i == 1)
      {
         file = "B";
      }
      if(i == 2)
      {
         file = "C";
      }
      if(i == 3)
      {
         file = "D";
      }
      if(_root.File == file)
      {
         _root["File" + file + "Star"] = deepObjectCopy(_root.Star);
         _root["File" + file + "Fludd"] = deepObjectCopy(_root.FluddArray);
         _root["File" + file + "StarCoin"] = deepObjectCopy(_root.StarCoin);
         _root["File" + file + "BowserKey1"] = deepObjectCopy(_root.BowserKey1);
         _root["File" + file + "BowserKey2"] = deepObjectCopy(_root.BowserKey2);
         _root["File" + file + "BowserKey3"] = deepObjectCopy(_root.BowserKey3);
         _root["File" + file + "DoorOpenedArray"] = deepObjectCopy(_root.DoorOpenedArray);
      }
      else
      {
         _root["File" + file + "Star"] = deepObjectCopy(_root.savedfiles.data["File" + file + "Star"]);
         _root["File" + file + "Fludd"] = deepObjectCopy(_root.savedfiles.data["File" + file + "Fludd"]);
         _root["File" + file + "StarCoin"] = deepObjectCopy(_root.savedfiles.data["File" + file + "StarCoin"]);
         _root["File" + file + "BowserKey1"] = deepObjectCopy(_root.savedfiles.data["File" + file + "BowserKey1"]);
         _root["File" + file + "BowserKey2"] = deepObjectCopy(_root.savedfiles.data["File" + file + "BowserKey2"]);
         _root["File" + file + "BowserKey3"] = deepObjectCopy(_root.savedfiles.data["File" + file + "BowserKey3"]);
         _root["File" + file + "DoorOpenedArray"] = deepObjectCopy(_root.savedfiles.data["File" + file + "DoorOpenedArray"]);
      }
      i++;
   }
   i = 0;
   while(i < 4)
   {
      if(i == 0)
      {
         file = "A";
      }
      if(i == 1)
      {
         file = "B";
      }
      if(i == 2)
      {
         file = "C";
      }
      if(i == 3)
      {
         file = "D";
      }
      _root.savedfiles.data["File" + file + "Star"] = deepObjectCopy(_root["File" + file + "Star"]);
      _root.savedfiles.data["File" + file + "Fludd"] = deepObjectCopy(_root["File" + file + "Fludd"]);
      _root.savedfiles.data["File" + file + "StarCoin"] = deepObjectCopy(_root["File" + file + "StarCoin"]);
      _root.savedfiles.data["File" + file + "BowserKey1"] = deepObjectCopy(_root["File" + file + "BowserKey1"]);
      _root.savedfiles.data["File" + file + "BowserKey2"] = deepObjectCopy(_root["File" + file + "BowserKey2"]);
      _root.savedfiles.data["File" + file + "BowserKey3"] = deepObjectCopy(_root["File" + file + "BowserKey3"]);
      _root.savedfiles.data["File" + file + "DoorOpenedArray"] = deepObjectCopy(_root["File" + file + "DoorOpenedArray"]);
      i++;
   }
   _root.savedfiles.data.FileExists = true;
   _root.savedfiles.flush();
   _root.CalculateFilesInfo();
};
_root.LoadFile = function()
{
   _root.savedfiles = SharedObject.getLocal("Ages2001SM63SerializationProbeV1");
   i = 0;
   while(i < 4)
   {
      if(i == 0)
      {
         file = "A";
      }
      if(i == 1)
      {
         file = "B";
      }
      if(i == 2)
      {
         file = "C";
      }
      if(i == 3)
      {
         file = "D";
      }
      _root["File" + file + "Star"] = deepObjectCopy(_root.savedfiles.data["File" + file + "Star"]);
      _root["File" + file + "Fludd"] = deepObjectCopy(_root.savedfiles.data["File" + file + "Fludd"]);
      _root["File" + file + "StarCoin"] = deepObjectCopy(_root.savedfiles.data["File" + file + "StarCoin"]);
      _root["File" + file + "BowserKey1"] = deepObjectCopy(_root.savedfiles.data["File" + file + "BowserKey1"]);
      _root["File" + file + "BowserKey2"] = deepObjectCopy(_root.savedfiles.data["File" + file + "BowserKey2"]);
      _root["File" + file + "BowserKey3"] = deepObjectCopy(_root.savedfiles.data["File" + file + "BowserKey3"]);
      _root["File" + file + "DoorOpenedArray"] = deepObjectCopy(_root.savedfiles.data["File" + file + "DoorOpenedArray"]);
      i++;
   }
   _root.Star = deepObjectCopy(_root["File" + _root.File + "Star"]);
   _root.FluddArray = deepObjectCopy(_root["File" + _root.File + "Fludd"]);
   _root.StarCoin = deepObjectCopy(_root["File" + _root.File + "StarCoin"]);
   _root.BowserKey1 = deepObjectCopy(_root["File" + _root.File + "BowserKey1"]);
   _root.BowserKey2 = deepObjectCopy(_root["File" + _root.File + "BowserKey2"]);
   _root.BowserKey3 = deepObjectCopy(_root["File" + _root.File + "BowserKey3"]);
   _root.DoorOpenedArray = deepObjectCopy(_root["File" + _root.File + "DoorOpenedArray"]);
   if(_root.DoorOpenedArray == undefined)
   {
      _root.DoorOpenedArray = [];
      i = 0;
      while(i < 14)
      {
         _root.DoorOpenedArray.push(false);
         i++;
      }
   }
   _root.CalculateStars();
   _root.CalculateStarCoins();
   _root.CalculateFilesInfo();
   i = 1;
   while(i < 12)
   {
      _root["RecordCoin" + i] = _root.savedfiles.data["RecordCoin" + i];
      i++;
   }
   _root.savedfiles.flush();
};
_root.CalculateStars = function()
{
   count = 1;
   _root.TotalStars = 0;
   while(_root.Star[count] !== undefined)
   {
      if(_root.Star[count] == true)
      {
         _root.TotalStars = _root.TotalStars + 1;
      }
      count++;
   }
   if(_root.CheatAllShines == true)
   {
      _root.TotalStars = 64;
   }
};
_root.CalculateStarCoins = function()
{
   count = 1;
   _root.TotalStarCoins = 0;
   while(_root.StarCoin[count] !== undefined)
   {
      if(_root.StarCoin[count] == true)
      {
         _root.TotalStarCoins = _root.TotalStarCoins + 1;
      }
      count++;
   }
   if(_root.CheatAllShines == true)
   {
      _root.TotalStarCoins = 64;
   }
};
_root.CalculateFilesInfo = function()
{
   _root.MaxStarCoins = 0;
   i = 0;
   while(i < 4)
   {
      if(i == 0)
      {
         file = "A";
      }
      if(i == 1)
      {
         file = "B";
      }
      if(i == 2)
      {
         file = "C";
      }
      if(i == 3)
      {
         file = "D";
      }
      count = 1;
      _root["File" + file + "TotalStars"] = 0;
      while(_root["File" + file + "Star"][count] !== undefined)
      {
         if(_root["File" + file + "Star"][count] == true)
         {
            _root["File" + file + "TotalStars"]++;
         }
         count++;
      }
      count = 1;
      _root["File" + file + "TotalStarCoins"] = 0;
      while(_root["File" + file + "StarCoin"][count] !== undefined)
      {
         if(_root["File" + file + "StarCoin"][count] == true)
         {
            _root["File" + file + "TotalStarCoins"]++;
         }
         count++;
      }
      if(_root["File" + file + "TotalStarCoins"] > _root.MaxStarCoins)
      {
         _root.MaxStarCoins = _root["File" + file + "TotalStarCoins"];
      }
      i++;
   }
   trace(_root.MaxStarCoins);
};

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
