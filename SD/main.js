var units = [
  { unit: "Volts",  SI: "V", dp: 1},
  { unit: "Watts", SI: "W", dp: 1},
  { unit: "Wh",  SI: "Wh", dp: 1},
  { unit: "Amps", SI: "A", dp: 2},
  { unit: "VA", SI: "VA", dp: 1},
  { unit: "PF", SI: "PF", dp: 2},
  { unit: "Hz", SI: "Hz", dp: 0},
  { unit: "VAR", SI: "VAR", dp: 1},
  { unit: "VARh", SI: "VARh", dp: 1}
];
// configuration files
var configFileURL = "/config.txt";
var configNewURL = "/config+1.txt";
var burdenFileURL = "/esp_spiffs/config/device/burden.txt";
var configTablesURL = "/tables.txt";
var editURL = "edit.htm";
var graphURL = "graph.htm";
var graph2URL = "graph2.htm";
var msgsFileURL = "/iotawatt/iotamsgs.txt?textpos=-10000";
var currentBody = [];
var configSHA256;
var noConfig = true;
var emoncmsAlias = "emoncms";       // Json object where Emoncms config is found (will be deprecated in future release)

var config;                         // configuration as Js Object
var serverConfig;
var tables;                         // tables as Js Object
var editing = false;
var editingScript = false;
var getStatus = false;
var calVoltageUpdate = false;
var avgVoltage = 0;
var calVTvolts = 0;
var originalName;
var timezone = 0;
var faults = 0;
var tokens = [{oper:"const",value:"0"}];
var calcDisplay = "0";
var parenLevel = 0;
var inputEditChannel;
var voltageChannels = [];
var vchanUsed = [];
var derivedTable = [{phase:"A",vphase:0,vmult:1},{phase:"B",vphase:120,vmult:1},{phase:"C",vphase:240,vmult:1},
                   {phase:"A-B",vphase:-30,vmult:1.732},{phase:"B-C",vphase:+90,vmult:1.732},{phase:"C-A",vphase:-150,vmult:1.732}];
var scriptEditTable;
var scriptEditSet;
var scriptEditIndex = -1;
var scriptEditSave;
var scriptEditReturn;
var scriptEditNameList = [];
var scriptEditNameTitle = "";
var scriptEditNamePattern = "^[a-zA-Z]{1}[a-zA-Z0-9_]{0,15}$";
var scriptEditUnits = [];
var scriptEditUniqueNames = true;
var scriptEditUnitsOutput = ["Watts","Volts","Amps","Hz","PF","VA","VAR","VARh"];
var scriptEditUnitsIntegrations = ["Wh", "VARh"];
var scriptEditUnitsUpload = ["Watts","Volts","Amps","Hz","PF","VA","Wh","kWh","VAR","VARh"];
var dstRules = [{id:"Americas",begOffset:-10,endOffset:-3.5,dstrule:{adj:60,utc:false,begin:{month:3,weekday:1,instance:2,time:120},end:{month:11,weekday:1,instance:1,time:120}}},
                {id:"Europe",begOffset:0,endOffset:2,dstrule:{adj:60,utc:true,begin:{month:3,weekday:1,instance:-1,time:60},end:{month:10,weekday:1,instance:-1,time:60}}},
                {id:"Australia",begOffset:9.5,endOffset:10.0,dstrule:{adj:60,utc:false,begin:{month:10,weekday:1,instance:1,time:120},end:{month:4,weekday:1,instance:1,time:180}}},
                {id:"NewZealand",begOffset:12,endOffset:12,dstrule:{adj:60,utc:false,begin:{month:9,weekday:1,instance:-1,time:120},end:{month:4,weekday:1,instance:1,time:180}}}
              ];
/***************************************************************************************************
 *                       Shorthand functions
* ************************************************************************************************/
function EbyId(id) {return document.getElementById(id)};

/***************************************************************************************************
*                       setProxy
* ************************************************************************************************/
var pattern_proxyURL = new RegExp('^(http:\\/\\/)?'+ // protocol
    '((([a-z\\d]([a-z\\d-]*[a-z\\d])*)\\.)+[a-z]{2,}|'+ // domain name
    '((\\d{1,3}\\.){3}\\d{1,3}))'+ // OR ip (v4) address
    '(\\:\\d+)?(\\/[-a-z\\d%_.~+]*)*$','i');  // port and path X

function setProxy(){
  editing = true;
  EbyId("proxyURL").value = (config.device.httpsproxy === undefined) ? "" : config.device.httpsproxy;
  validateProxy();
  currentBodyPush("setProxy");
}

function validateProxy(){
  if(pattern_proxyURL.test(EbyId("proxyURL").value)){
    show("proxySave");
  }
  else{
    hide("proxySave");
  }
}

function proxyDelete(){
  EbyId("proxyURL").value = "";
  proxySave();
}

function proxyCancel(){
  editing = false;
  currentBodyPop();
  resetDisplay();
}

function proxySave(){
  config.device.httpsproxy = EbyId("proxyURL").value;
  if(config.device.httpsproxy == ""){
    config.device.httpsproxy = undefined;
  }
  uploadConfig();
  editing = false;
  currentBodyPop();
  resetDisplay();
}

/***************************************************************************************************
*                       setPasswords
* ************************************************************************************************/
var authReq = {};
function setPasswords() {
  editing = true;
  authReq = {};
  EbyId("pwdTable").innerHTML = "Checking for existing password";
  currentBodyPush("setPasswords");
  var xmlHttp = new XMLHttpRequest();
  xmlHttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var response = JSON.parse(xmlHttp.responseText);
      if(response.passwords.admin){
        EbyId("pwdTable").innerHTML = "";
        var oldAdmin = addTableRow(pwdTable, "Current Admin password", "oldAdminPwd", "input", 16);
        oldAdmin.setAttribute("onblur","pwdValidate();");
        oldAdmin.title = "Required to proceed";
        show("pwdCheck");
        //EbyId("pwdCheck").style.display = "inline";
      } else {
        hide("pwdCheck");
        //EbyId("pwdCheck").style.display = "none";
        pwdValid();
      }
    }
  }
  xmlHttp.open("GET","/status?passwords=yes", true);
  xmlHttp.send(null);
  hide("pwdCheck");
  //EbyId("pwdSave").style.display = "none";
}
 
function pwdValidate(){
  var xmlHttp = new XMLHttpRequest();
  xmlHttp.onreadystatechange = function() {
    if (this.readyState == 4) {
      if(this.status == 200) {
        pwdValid();
      } else {
        alert("Not current password");
      }
    }
  }
  xmlHttp.open("POST","/auth");
  var oldAdmin = EbyId("oldAdminPwd");
  authReq.oldadmin = oldAdmin.value;
  xmlHttp.send(btoa(JSON.stringify(authReq)));
}

function pwdValid(){
  EbyId("pwdTable").innerHTML = "";
  EbyId("pwdCheck").style.display = "none";
  var newPwd = addTableRow(pwdTable, "new Admin password", "newAdminPwd", "input", 16);
  newPwd.pattern = "^[A-Za-z0-9_!%@#]{0,16}";
  newPwd.title = "1-16 characters, blank to reset";
  newPwd.setAttribute("oninput","pwdCheck();");
  newPwd = addTableRow(pwdTable, "new User password", "newUserPwd", "input", 16);
  newPwd.pattern = "^[A-Za-z0-9_!%@#]{0,16}";
  newPwd.title = "1-16 characters, blank to reset";
  newPwd.setAttribute("oninput","pwdCheck();");
  localAccess = addTableRow(pwdTable, "Unrestricted LAN access", "localAccess", "input");
  localAccess.setAttribute("type","checkbox");
  localAccess.title = "No passwords needed from LAN.";
  pwdCheck();
}

function pwdCheck(){
  var complete = true;
  if( ! validateInput("newAdminPwd", false, "not a valid password")) complete = false;
  if( ! validateInput("newUserPwd", false, "not a valid password")) complete = false;
  if(complete){
    EbyId("pwdSave").style.display = "inline";
  }
}

function pwdCancel(){
  editing = false;
  currentBodyPop();
}

function pwdSave(){
  var xmlHttp = new XMLHttpRequest();
  xmlHttp.onreadystatechange = function() {
    if (this.readyState == 4) {
      if(this.status == 200) {
        editing = false;
        currentBodyPop();
        return;
      } else {
        alert("update failed");
      }
    }
  }
  xmlHttp.open("POST","/auth");
  authReq.newadmin = EbyId("newAdminPwd").value;
  authReq.newuser = EbyId("newUserPwd").value;
  authReq.localAccess = EbyId("localAccess").checked;
  xmlHttp.send(btoa(JSON.stringify(authReq)));
}

/***************************************************************************************************
 *                       Tools
* ************************************************************************************************/
function toolsRestart() {
  EbyId("restartMsg").innerHTML = "";
  currentBodyPush("divRestart");
}

function sendRestart(){
  var xmlHttp = new XMLHttpRequest();
  xmlHttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      editing = true;
      EbyId("restartMsg").innerHTML = "Restarting...";
      setTimeout(function(){setup();},15000);
    }
  }
  xmlHttp.open("GET","/command?restart=yes", true);
  xmlHttp.send(null);
}

function toolsWiFi() {
  EbyId("WiFiMsg").innerHTML = "";
  currentBodyPush("divWiFi");
}

function sendDisconnect(){
  var xmlHttp = new XMLHttpRequest();
  xmlHttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      editing = true;
      EbyId("WiFiMsg").innerHTML = "Disconnected";
      setTimeout(function(){setup();},15000);
    }
  }
  xmlHttp.open("GET","/command?disconnect=yes", true);
  xmlHttp.send(null);
}

function toolsDatalogs(){
  currentBodyPush("divLogs");
  toolsDatalogsStatus();
}

function toolsDataLogsCancel(){
  EbyId("logsConfirm").onclick="";
  EbyId("logsMsg").innerHTML = "";
  EbyId("logsConfirm").style.display = "none";
  editing = false;
  toggleDisplay("logsDelete");
}


function toolsDatalogsStatus(){
  var xmlHttp = new XMLHttpRequest();
  xmlHttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var status = JSON.parse(xmlHttp.responseText);
      EbyId("logsTable").innerHTML = "<tr><th>Attribute</th><th>Current Log</th><th>History Log</th></tr>";
      formatLine("Start Date",formatDateTime(status.datalogs.currlog.firstkey),formatDateTime(status.datalogs.histlog.firstkey));
      formatLine("End Date",formatDateTime(status.datalogs.currlog.lastkey),formatDateTime(status.datalogs.histlog.lastkey));
      formatLine("File Size",status.datalogs.currlog.size,status.datalogs.histlog.size);
      formatLine("Interval (sec)",status.datalogs.currlog.interval,status.datalogs.histlog.interval);
      formatLine("Complete", density(status.datalogs.currlog) + "%", density(status.datalogs.histlog) + "%");
      return;            
      function formatLine(attribute, value1, value2){
        EbyId("logsTable").innerHTML += "<tr><td>" + attribute + "</td><td>" + value1 + "</td><td>" + value2 + "</td></tr>";
      }
      function density(log){
        return ( (log.size / 256) * 100 /((log.lastkey-log.firstkey)/ log.interval)).toFixed(1);
      }
    }
  }
  xmlHttp.open("GET","/status?datalogs=yes", true);
  xmlHttp.send(null);
}

function toolsDataLogsCommit(log){
  var xmlHttp = new XMLHttpRequest();
  xmlHttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var msg = EbyId("logsMsg");
      msg.innerHTML = "Deleting " + log + " and restarting.";
      setTimeout(function(){toolsDataLogsCancel(); setup();},15000);
    }
  }
  var uri = "/command?deletelog=" + log;
  xmlHttp.open("GET", uri, true);
  xmlHttp.send(null);
}

function toolsDataLogsDelete(log){
  var msg = EbyId("logsMsg");
  msg.innerHTML = "You are about to permanently delete the ";
  if(log == "current"){
    msg.innerHTML += "current log.";
    EbyId("logsConfirm").setAttribute("onclick","toolsDataLogsCommit('current')");
  }
  else if (log == "history"){
    msg.innerHTML += "history log.";
    EbyId("logsConfirm").setAttribute("onclick","toolsDataLogsCommit('history')");
  }
  else {
    toolsDataLogsCancel();
    return;
  }
  EbyId("logsConfirm").style.display = "block";
  editing = true;
}

/***************************************************************************************************
 *                        Configure Inputs
 * ************************************************************************************************/
function configInputs(){
  currentBodyPush("configInputs");
  var inputsTable = EbyId("inputTableBody");
  inputsTable.innerHTML = "";
  voltageChannels = [];
  EbyId("derive3ph").disabled = false;
  for(i in config.inputs){
    vchanUsed[i] = false;
    if(config.inputs[i] !== null){
      if(config.inputs[i].type == "VT"){
        voltageChannels.push(i);
      }
      if(config.inputs[i].vphase !== undefined && config.inputs[i].vphase != 0){
        config.derive3ph = true;
        EbyId("derive3ph").disabled = true;
      }
    }
  }
  EbyId("derive3ph").checked = config.derive3ph ? true : false;
  for( i in config.inputs){
    var newRow = inputsTable.insertRow(-1);
    newRow.setAttribute("class","chanEditRow");
    var newColumn = newRow.insertCell(-1);
    var inputButton = document.createElement("button");
    inputButton.setAttribute("class","chanButton");
    inputButton.setAttribute("onclick","inputEdit(" + i +")");
    inputButton.innerHTML = i;
    newColumn.appendChild(inputButton);
    nameColumn = newRow.insertCell(-1);
    newColumn = newRow.insertCell(-1);
    if(config.inputs[i] !== null){
      nameColumn.innerHTML += "<strong>" + config.inputs[i].name + "</strong>";
      newColumn.innerHTML = "<small><strong>" + config.inputs[i].type + "</strong>";
      newColumn.innerHTML += ", <strong>" + config.inputs[i].model + "</strong>";
      if(config.inputs[i].double){
        newColumn.innerHTML += "(x2)";
      }
      if(config.inputs[i].reverse){
        newColumn.innerHTML += "&#8634";
      }
      if(config.inputs[i].turns != undefined){
        config.inputs[i].cal = config.inputs[i].turns / config.device.burden[i];
      }
      if(config.inputs[i].vchan !== undefined){
        vchanUsed[config.inputs[i].vchan] = true;
        if(config.inputs[i].vchan != 0){
          newColumn.innerHTML += ", Vref:<strong>" + config.inputs[i].vchan + "</strong>";
        }
      }
      
      if(config.derive3ph){
        if(config.inputs[i].vphase === undefined){
          config.inputs[i].vphase = 0;
        }
        if(config.inputs[i].vmult === undefined){
          config.inputs[i].vmult = 1;
        }
        for(j in derivedTable){
          if(derivedTable[j].vphase == config.inputs[i].vphase){
            newColumn.innerHTML += ", phase:<strong>" + derivedTable[j].phase + "</strong>";
            break;
          }
        }
      }
    }
  }
}

function derive3ph(){
  config.derive3ph = !config.derive3ph;
  uploadConfig();
  configInputs();
}

function inputEdit(channel){
  currentBodyPush("inputEdit");
  inputEditObject = config.inputs[channel];
  inputEditChannel = channel;
  editing = true;
  if(inputEditObject === null){
    inputEditObject = {channel:channel, name:"Input_" + channel};
    inputEditNewType("CT");
  }
  refreshInputEdit();
}

function inputEditMsg(element, msg){
  element.parentNode.appendChild(document.createElement("br"));
  element.parentNode.appendChild(document.createTextNode(msg));
}

function refreshInputEdit(){
  var complete = true;
  var table = EbyId("inputEditTable");
  table.innerHTML = "";
  EbyId("inputChannel").innerHTML = "Configure Input " + inputEditChannel;
  
  var newInput = addTableRow(table, "Burden:", "inputBurden", "span");
  if(config.device.burden[inputEditChannel] == 0) inputBurden.innerHTML = "none configured.";
  else if(Number.isInteger(config.device.burden[inputEditChannel])) inputBurden.innerHTML = config.device.burden[inputEditChannel].toFixed(0) + " ohms";
  else inputBurden.innerHTML = config.device.burden[inputEditChannel].toFixed(1) + " ohms";
  
  var inputName = addTableRow(table, "Name: ", "inputName", "input", 12);
  inputName.setAttribute("onblur","inputEditObject.name=this.value.trim(); refreshInputEdit();");
  inputName.value = inputEditObject.name;
  inputName.pattern = "[A-Za-z]{1}[a-zA-Z0-9_]*";
  
  if(inputName.value == ""){
    inputEditMsg(inputName, "Please specify a name");
    complete = false;
  }
  if(inputName.validationMessage != ""){
    inputEditMsg(inputName, "Invalid name");
    complete = false;
  }
  for(i in config.inputs){
    if(i != inputEditChannel && config.inputs[i] !== null && config.inputs[i].name == inputName.value){
      inputName.parentNode.appendChild(document.createElement("br"));
      keyInput.parentNode.appendChild(document.createTextNode(" API key should be 16 hex digits."));
      inputEditMsg(inputName, "Name already used for channel " + inputEditChannel);
      complete = false;
    }
  }
  
  newInput = addTableRow(table, "Type: ", "inputType", "select");
  newInput.setAttribute("onchange","inputEditNewType(this.value); refreshInputEdit();");
  var option = document.createElement("option");
  option.text = inputEditObject.type;
  option.selected = true;
  newInput.add(option); 
  option = document.createElement("option");
  option.text = (inputEditObject.type == "CT") ? "VT" : "CT";
  newInput.add(option);
  
  inputModel = addTableRow(table, "Model: ", "inputModel", "select");
  var option = document.createElement("option");
  option.text = "generic";
  inputModel.add(option); 
  if(inputEditObject.type == "VT"){
    for(i in tables.VT){
      option = document.createElement("option");
      option.text = tables.VT[i].model;
      if(tables.VT[i].mfg !== undefined){
        option.title = tables.VT[i].mfg;
      }
      inputModel.add(option);
      if(inputEditObject.model == option.text){
        option.selected = true;
      } 
    }
  }
  else {
    for(i in tables.CT){
      option = document.createElement("option");
      option.text = tables.CT[i].model;
      if(tables.CT[i].mfg !== undefined){
        option.title = tables.CT[i].mfg;
      }
      if(tables.CT[i].type == "C" && config.device.burden[inputEditChannel] > 0){
        inputModel.add(option);
        if(inputEditObject.model == option.text){
          option.selected = true;
          inputEditObject.turns = tables.CT[i].turns;
          inputEditObject.phase = tables.CT[i].phase;
          inputEditObject.cal = (inputEditObject.turns / config.device.burden[inputEditChannel]).toPrecision(4);
        }
      }
      else if(tables.CT[i].type == "V" && config.device.burden[inputEditChannel] == 0){
        inputModel.add(option);
        if(inputEditObject.model == option.text){
          option.selected = true;
          inputEditObject.cal = tables.CT[i].cal;
          inputEditObject.phase = tables.CT[i].phase;
        } 
      }
    }
  }
  inputModel.setAttribute("onchange","inputEditNewModel(this)");
  
  if(inputEditObject.model == "generic" && 
    (inputEditObject.type == "VT" ||
    (inputEditObject.type == "CT" && config.device.burden[inputEditChannel] == 0))){
    inputCal = addTableRow(table, "Cal: ", "inputCal", "input", 5);
    inputCal.setAttribute("onblur","inputEditObject.cal=this.value.trim(); refreshInputEdit();");
     if(inputEditObject.cal === undefined){
      complete = false;
    } 
    else {
      inputCal.value = inputEditObject.cal;
      inputCal.value = parseFloat(inputCal.value).toPrecision(4);
      if(inputCal.value == 0){
        inputEditMsg(inputCal,"Cannot be zero");
        complete = false;
      }
    }
  }
  
  if(inputEditObject.model == "generic" && inputEditObject.type == "CT" && config.device.burden[inputEditChannel] > 0){
    inputTurns = addTableRow(table, "Turns: ", "inputTurns", "input", 5);
    inputTurns.setAttribute("onblur","inputEditObject.turns = this.value.trim(); refreshInputEdit();");
    inputTurns.value = inputEditObject.turns;
    if(Number.isNaN(inputTurns.value)) {
      inputEditMsg(inputTurns, "Numeric value required");
      complete = false;
    }
    else {
      inputTurns.value = parseInt(inputTurns.value);
      var cal = inputTurns.value / config.device.burden[inputEditChannel];
      if(cal < 5 || cal >= 1000) {
        inputEditMsg(inputTurns, " Range is " + (config.device.burden[inputEditChannel] * 5).toString() + " to " + (config.device.burden[inputEditChannel] * 900).toString());
        complete = false;
      } else {
        inputEditObject.cal = cal;
      }
    }
  }
  
 //************************************************************************************************************************************
  if(inputEditObject.model == "generic"){
    inputPhase = addTableRow(table, "Phase lead: ", "inputPhase", "input", 4);
    inputPhase.setAttribute("onblur","inputEditObject.phase = this.value; refreshInputEdit();");
    inputPhase.value = (inputEditObject.phase !== undefined) ? inputEditObject.phase : 0;
    if(isNaN(inputEditObject.phase)){
      inputEditMsg(inputPhase, "Numeric value required");
      complete = false;
    }
  }
  
  if(inputEditObject.type == "CT"){
    if(voltageChannels.length > 1){
      inputVchan = addTableRow(table, "VRef", "inputVchan", "select");
      inputVchan.setAttribute("oninput","inputEditObject.vchan = Number(this.value); refreshInputEdit();");
      for(i in voltageChannels){
        option = document.createElement("option");
        option.value = voltageChannels[i];
        option.text = config.inputs[voltageChannels[i]].name;
        inputVchan.add(option);
        if(inputEditObject.vchan !== undefined && inputEditObject.vchan == voltageChannels[i]){
          option.selected = true;
        } 
      }
    }
    if(config.derive3ph){
      inputVchan = addTableRow(table, "Mains Phase", "inputVphase", "select");
      inputVchan.setAttribute("oninput",
        "inputEditObject.vphase=derivedTable[this.value].vphase; inputEditObject.vmult=derivedTable[this.value].vmult; refreshInputEdit();");
      for(i in derivedTable){
        option = document.createElement("option");
        option.value = Number(i);
        option.innerHTML = derivedTable[i].phase;
        if(derivedTable[i].vphase != 0){
          option.innerHTML += " (";
          option.innerHTML += (derivedTable[i].vphase >= 0) ? "+" : "";
          option.innerHTML += derivedTable[i].vphase + "&#176)";
        }
        inputVchan.add(option);
        if(inputEditObject.vphase !== undefined && inputEditObject.vphase == derivedTable[i].vphase){
          option.selected = true;
        } 
      }
    }
  }
  
  if(inputEditObject.type == "CT"){
    
    inputNeg = addTableRow(table, "", "inputNeg", "input");
    inputNeg.parentNode.appendChild(document.createTextNode("Allow negative power value"));
    addInfo(inputNeg, "Use on mains when solar or wind can export power.");
    inputNeg.setAttribute("type","checkbox");
    inputNeg.setAttribute("onchange","inputEditObject.signed = this.checked; refreshInputEdit();");
    if(inputEditObject.signed != undefined && inputEditObject.signed){
      inputNeg.checked = true;
    }
    else {
      inputEditObject.signed = undefined;
    }
    
    if( ! config.derive3ph || inputEditObject.double != undefined){
      inputDouble = addTableRow(table, "", "inputMult", "input");
      inputDouble.parentNode.appendChild(document.createTextNode("Double"));
      addInfo(inputDouble, "Double measured power.  Use with balanced 240V circuit with CT on one conductor.");
      inputDouble.setAttribute("type","checkbox");
      inputDouble.setAttribute("onchange","inputEditObject.double = this.checked; refreshInputEdit();");
      if(inputEditObject.double != undefined && inputEditObject.double){
        inputDouble.checked = true;
      } else {
        inputEditObject.double = undefined;
      }
    }
    
    EbyId("inputEditDelete").style.display = (config.inputs[inputEditChannel] == null) ? "none" : "inline";
  }
  
  inputReverse = addTableRow(table, "", "inputReverse", "input");
  inputReverse.parentNode.appendChild(document.createTextNode("Reverse"));
  addInfo(inputReverse, inputEditObject.type == "CT" ? "Same as reversing CT on conductor" : "Same as reversing VT in socket");
  inputReverse.setAttribute("type","checkbox");
  inputReverse.setAttribute("onchange","inputEditObject.reverse = this.checked; refreshInputEdit();");
  if(inputEditObject.reverse != undefined && inputEditObject.reverse){
    inputReverse.checked = true;
  } else {
    inputEditObject.reverse = undefined;
  }

  if(inputEditObject.type == "VT"){
    EbyId("inputEditDelete").style.display = 
      (config.inputs[inputEditChannel] == null || vchanUsed[inputEditChannel]) ? "none" : "inline";
    if(complete){
      inputCal = addTableRow(table, "", "inputCal", "button");
      inputCal.innerHTML = "calibrate";
      inputCal.setAttribute("class","actionCal");
      inputCal.setAttribute("onclick","calVTvoltage();");
    }
  }
  
  if(complete) show("inputEditSave");
  else hide("inputEditSave");
}

function addInfo(element, title){
  infoSymbol = document.createElement("span");
  infoSymbol.innerHTML = " &#128712";
  infoSymbol.title = title;
  element.parentNode.appendChild(infoSymbol);
}

function inputEditNewModel(obj){
  inputEditObject.model = obj.value;
  if(inputEditObject.model == "generic"){
    inputEditNewType(inputEditObject.type);
  }
  else if(inputEditObject.type == "VT"){
    for(i in tables.VT){
      if(tables.VT[i].model == obj.value){
        inputEditObject.cal = tables.VT[i].cal;
        inputEditObject.phase = tables.VT[i].phase;
        break;
      }
    }
  }
  else {
    for(i in tables.CT){
      if(tables.CT[i].model == obj.value){
        inputEditObject.phase = tables.CT[i].phase;
        if(config.device.burden[inputEditChannel] == 0){
          inputEditObject.cal = tables.CT[i].cal;
        }
        else {
          inputEditObject.turns = tables.CT[i].turns;
          inputEditObject.cal = (tables.CT[i].turns / config.device.burden[inputEditChannel]).toPrecision(4);
        }
        break;
      }
    }
  }
  refreshInputEdit();
}

function inputEditNewType(type){
  inputEditObject = {channel:inputEditChannel, name:inputEditObject.name, type:type, model:"generic", phase:0,};
  if(type == "CT"){
     if(config.device.burden[inputEditChannel] == 0){
       inputEditObject.cal = 0;
     }
     else {
       inputEditObject.turns = "0";
     }
   }
   refreshInputEdit();
}

function inputEditDelete(){
  inputEditObject = null;
  inputEditSave();
}

function inputEditCancel(){
  currentBodyPop();
  getConfig(configInputs);
  editing = false;
}

function inputEditSave(){
  if(inputEditObject && inputEditObject.vphase !== undefined && inputEditObject.vphase == 0){
    inputEditObject.vphase = undefined;
  }
  config.inputs[inputEditChannel] = inputEditObject;
  editing = false;
  currentBodyPop();
  uploadConfig();
  configInputs();
}

      // This is used by other sections as well

function addTableRow(table, text, id, elementType, size) {
  var newRow = table.insertRow(-1);
  var column = newRow.insertCell(-1);
  column.innerHTML = text;
  column.setAttribute("align","right");
  column = newRow.insertCell(-1);
  var newInput = document.createElement(elementType);
  column.appendChild(newInput);
  if(id !== null) newInput.setAttribute("id",id);
  if(size !== undefined) newInput.setAttribute("size",size);
  return newInput;
}

function addTableText(table,col1Text,col2Text){
  var newRow = table.insertRow(-1);
  var col1 = newRow.insertCell(-1);
  col1.innerHTML = col1Text;
  col1.setAttribute("align","right");
  col2 = newRow.insertCell(-1);
  col2.innerHTML = col2Text;
  return col2;
}

/**********************************************************************************************************************************
 * 
 *  Voltage calibration
 * 
 * *******************************************************************************************************************************/

function calVTvoltage(obj){
  currentBodyPush("inputCalVT");
  EbyId("inputCalChannel").innerHTML = "Calibrate Voltage Channel " + inputEditChannel;
  EbyId("inputCalVolts").innerHTML = "120.1";
  EbyId("inputCalCal").value = inputEditObject.cal;
  
  calRefreshVoltage = true;
  calVTvolts = 0;
  calUpdateVoltage();
}

function calVTexit(){
  var index = inputEditChannel;
  currentBodyPop();
  calRefreshVoltage = false;
  refreshInputEdit();
}

function inputCalCal(obj){
  var step = (obj.value / 8).toFixed(0) / 100;
  obj.setAttribute("step",step);
}

function calUpdateVoltage(){
  var index = inputEditChannel;
  var xmlHttp = new XMLHttpRequest();
  xmlHttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      if(calRefreshVoltage){
        var response = JSON.parse(xmlHttp.responseText);
        if(calVTvolts == 0) calVTvolts = response.vrms;
        else calVTvolts = .8 * calVTvolts + .2 * response.vrms;
        EbyId("inputCalVolts").innerHTML = calVTvolts.valueOf().toFixed(1);
        calUpdateVoltage();
      }
    }
  }
  xmlHttp.open("GET","/vcal?channel=" + inputEditChannel + "&cal=" + Number(EbyId("inputCalCal").value), true);
  xmlHttp.send(null);
}

function calVTsave(obj){
  inputEditObject.cal = Number(EbyId("inputCalCal").value);
  currentBodyPop();
  calRefreshVoltage = false;
  inputEditSave();
}

/*******************************************************************************************
 *                    Configure Outputs
 * ****************************************************************************************/
 function configOutputs(){
    currentBodyPush("configOutputs");
    
    scriptEditSave = function(){
      config.outputs = scriptEditSet
      uploadConfig();
    };
    scriptEditReturn = function(){
      EbyId("divCalc").style.display = "none";
      EbyId("divOutputs").style.display = "table";
      configOutputs();
    };
    scriptEditTable = EbyId("outputTable");
    scriptEditTable.innerHTML = "";
    scriptEditSet = config.outputs;
    scriptEditUnits = scriptEditUnitsOutput;
    scriptEditNameList = [];
    scriptEditUniqueNames = true;
    scriptEditNameTitle="Name must be alphanumeric and start with a letter";
    scriptEditNamePattern="^[a-zA-Z]{1}[a-zA-Z0-9_]{0,15}$";
    editScript();
  }
  
  /**********************************************************************************************
   *                editScript() - Generic Output Script Editor with "calculator"
   * 
   *                Caller must initialize these global variables.
   * 
   *                scriptEditTable - the table element in which to build the list
   *                scriptEditSet - the array containing the individual Scripts
   *                scriptEditSave - function  to be used to save the edited scriptSet
   *                scriptEditReturn - function to be called upon completion of an edit.
   *                scriptEditUnits - array of units values that are permitted
   *                
   *                scriptEditNameList - array of acceptable names values (empty = input)
   *                scriptEditNamePattern - regex pattern for validation
   *                scriptEditNameTitle - title to be associated with name
   *                
   * *******************************************************************************************/
   
  function editScript(){
    scriptEditIndex = -1;
    table = scriptEditTable;
    table.innerHTML = "";
    calcBuildDropdown();
    for(i in scriptEditSet){
      var newRow = document.createElement("tr");
      table.appendChild(newRow);
      newRow.setAttribute("class","outputsRow")
      var selColumn = document.createElement("td");
      newRow.appendChild(selColumn);
      var nameColumn = document.createElement("td");
      newRow.appendChild(nameColumn);
      var unitsColumn = document.createElement("td");
      newRow.appendChild(unitsColumn);
      var scriptColumn = document.createElement("td");
      newRow.appendChild(scriptColumn);
      nameColumn.innerHTML = "<strong>" + scriptEditSet[i].name + "</strong>";
      if(scriptEditSet[i].units === undefined) scriptEditSet[i].units = "";
      unitsColumn.innerHTML = scriptEditSet[i].units;
      scriptColumn.innerHTML = " = " + scriptDisplay(parseScript(scriptEditSet[i].script));
      var editButton = document.createElement("button");
      editButton.setAttribute("class","outputEditButton");
      editButton.appendChild(document.createTextNode("edit"));
      editButton.setAttribute("onclick","calculator(" + i +")");
      selColumn.appendChild(editButton);
    }
    var newRow = document.createElement("tr");
    table.appendChild(newRow);
    newRow.setAttribute("class","outputsRow")
    var selColumn = document.createElement("td");
    newRow.appendChild(selColumn);
    var editButton = document.createElement("button");
    editButton.setAttribute("class","outputEditButton");
    editButton.appendChild(document.createTextNode("add"));
    editButton.setAttribute("onclick","addOutput()");
    selColumn.appendChild(editButton);
  }
   
  
  function calcBuildDropdown(){
    var inputDropdown = EbyId("calcInputDropdown-content");
    inputDropdown.innerHTML = "";
    for(i in config.inputs){
      if(config.inputs[i] != null){
        var newItem = document.createElement("p");
        inputDropdown.appendChild(newItem);
        newItem.innerHTML = config.inputs[i].name;
        newItem.value = Number(i);
        newItem.setAttribute("onclick", "keyInput(this)");
      }
    }
  }

  function calculator(index){
    editingScript = true;
    currentBodyPush("divCalc");
    EbyId("calcTable").innerHTML = "";
    buildNames(scriptEditSet[index].name);
    buildUnitsList(scriptEditSet[index].units);
    tokens = parseScript(scriptEditSet[index].script);
    EbyId("calcDelete").style.display = "inline";
    scriptEditIndex = index;
    refreshCalcDisplay();
  }
 
  function addOutput(){
    editingScript = true;
    currentBodyPush("divCalc");
    EbyId("calcTable").innerHTML = "";
    buildNames("");
    buildUnitsList("watts");
    tokens = ["#0"];
    EbyId("calcDelete").style.display = "none";
    scriptEditIndex = scriptEditSet.length;
    refreshCalcDisplay();
  }

  function calcCancel(){
    currentBodyPop();
    editingScript = false;
    scriptEditReturn();
  }

  function calcSave(){
    if(scriptEditIndex < scriptEditSet.length){
      scriptEditSet.splice(scriptEditIndex,1);
    }
    scriptEditSet.push({name:EbyId("calcName").value.trim(),
                        units:EbyId("calcUnits").value.trim(),
                        script:tokens.join("") 
                      });
    scriptEditSet.sort(function(a, b){return a.name.toString().localeCompare(b.name.toString())});
    if(scriptEditUniqueNames){
      for(var i=0; i<scriptEditSet.length-1; i++){
        if(scriptEditSet[i].name == scriptEditSet[i+1].name) scriptEditSet.splice(i,1);
      }
    }
    scriptEditSave();
    currentBodyPop();
    editingScript = false;
    scriptEditReturn();
  }
  
  function calcDelete(){
    scriptEditSet.splice(scriptEditIndex,1);
    scriptEditSave();
    currentBodyPop();
    editingScript = false;
    scriptEditReturn();
  }

  function refreshCalcDisplay(){
    EbyId("calcDisplay").innerHTML = scriptDisplay(tokens);
    var calcName = EbyId("calcName");
    if(EbyId("calcName").value.trim() == "" ||
      parenLevel > 0 || 
      RegExp("^[-+*\/<>]").test(tokens[tokens.length-1])){
      EbyId("calcSave").style.display = "none";
    }
    else if (! RegExp(calcName.pattern).test(calcName.value)){
      EbyId("calcSave").style.display = "none";
    }
    else EbyId("calcSave").style.display = "inline";
  }
  
  function buildNames(name){
    var calcTable = EbyId("calcTable");
    if(scriptEditNameList.length == 0){
      var calcName = addTableRow(calcTable, "Name:", "calcName", "input", 1);
      calcName.setAttribute("onchange","refreshCalcDisplay()");
      calcName.pattern = scriptEditNamePattern;
      calcName.title = scriptEditNameTitle;
      calcName.value = name;
    } else {
      var calcName = addTableRow(calcTable, "Name:", "calcName", "select", 1);
      calcName.setAttribute("oninput","refreshCalcDisplay()");
      calcName.value = name;
      for(i in scriptEditNameList){
        var option = document.createElement("option");
        option.value = scriptEditNameList[i];
        option.innerHTML = scriptEditNameList[i];
        calcName.add(option);
        if(option.value == name){
          option.selected = true;
        }
      }
    }
  }
  
  function buildUnitsList(units){
    var calcTable = EbyId("calcTable");
    var calcUnits = addTableRow(calcTable, "Units:", "calcUnits", "select", 1);
    calcUnits.style = "width: 80px;";
    for(i in scriptEditUnits){
      var option = document.createElement("option");
      option.value = scriptEditUnits[i];
      option.innerHTML = scriptEditUnits[i];
      calcUnits.add(option);
      if(option.value == units){
        option.selected = true;
      }
    }
  }
  
  function parseScript(script){
    return script.match(/@\d+|#-?\d+\.?\d*|![a-zA-Z\d]+|[-+*\/()|<>=]/g);
  }

  function scriptDisplay(tokens){
    calcDisplay = "";
    for(var i=0; i<tokens.length; i++){
      if(tokens[i].startsWith("#") || tokens[i].startsWith('!')){
        calcDisplay += tokens[i].substr(1);
      }
      else if(tokens[i].startsWith("@")){
        var channel = tokens[i].substr(1);
        var name = "Input_" + channel;
        if(config.inputs[channel] != null) name = config.inputs[channel].name;
        if(name == "") name = "Input_" + channel;
        calcDisplay += name;
      }
      else if(tokens[i] == "*") calcDisplay += " x ";
      else if(tokens[i] == "+") calcDisplay += " + ";
      else if(tokens[i] == "-") calcDisplay += " - ";
      else if(tokens[i] == "/") calcDisplay += " &#247 ";
      else if(tokens[i] == "(") calcDisplay += "(";
      else if(tokens[i] == ")") calcDisplay += ")";
      else if(tokens[i] == "|") calcDisplay += " abs ";
      else if(tokens[i] == "<") calcDisplay += " min ";
      else if(tokens[i] == ">") calcDisplay += " max ";
    }
    return calcDisplay;
  }

  function keyClearAll(){
    while(tokens.length > 1){
      keyClearEntry();
    }
    keyClearEntry();
  }
  
  function keyClearEntry(){
    if(tokens[tokens.length-1] == ")"){
      parenLevel++;
    }
    else if(tokens[tokens.length-1] == "("){
      parenLevel--;
    }
    if(tokens.length > 1){
      tokens.pop();
    }
    else {
      tokens[0] = "#0";
    }
    refreshCalcDisplay();
  }
  
  function keyClearChar(){
    if(RegExp("^[-+*\/()|@<>]").test(tokens[tokens.length-1])){
      keyClearEntry();
    }
    else if(RegExp("#-?[0-9]$").test(tokens[tokens.length-1])){
       keyClearEntry();
    }
    else {
      tokens[tokens.length-1] = tokens[tokens.length-1].substr(0,tokens[tokens.length-1].length-1);
    }
    refreshCalcDisplay();
  }
  
  function keyDigit(digit){
    if(RegExp("^#0$").test(tokens[tokens.length-1])){
      tokens[tokens.length-1] = "#" + digit;
    }
    else if(RegExp("^#").test(tokens[tokens.length-1])){
      tokens[tokens.length-1] += digit;
    }
    else if(RegExp("^[-+*\/<>(]").test(tokens[tokens.length-1])){
      tokens.push("#" + digit);
    }
    refreshCalcDisplay();
  }
  
  function keyDecimal(){
    if(RegExp("^#-?[0-9]*$").test(tokens[tokens.length-1])){
      tokens[tokens.length-1] += ".";
    }
    else if(RegExp("^[-+*\/<>(]").test(tokens[tokens.length-1])){
      tokens.push("#0.");
    }
    refreshCalcDisplay();
  }
  
  function keyUniNeg(){
    if(RegExp("^#[0-9]+\.?[0-9]*").test(tokens[tokens.length-1])){
      tokens[tokens.length-1] = tokens[tokens.length-1].charAt(0) + "-" + tokens[tokens.length-1].substr(1);
    }
    else if(RegExp("^#\-[0-9]+\.?[0-9]*").test(tokens[tokens.length-1])){
      tokens[tokens.length-1] = tokens[tokens.length-1].charAt(0) + tokens[tokens.length-1].substr(2);
    }
    refreshCalcDisplay();
  }
  
  function keyUniPos(){
    if(RegExp("[@)]").test(tokens[tokens.length-1])){
       tokens.push("|");
     }
     refreshCalcDisplay();
  }
  
  function keyBinFunc(op){
    if(RegExp("^[@#)\|]").test(tokens[tokens.length-1])){
      tokens.push(op);
    }
    else if(RegExp("^[-+*\/<>]").test(tokens[tokens.length-1])){
      tokens[tokens.length-1] = op;
    }
    refreshCalcDisplay();
  }
  
  function keyPush(){
    if(tokens.length == 1 && tokens[0] == "#0") {
        tokens[0] = "(";
        parenLevel++;
      }
    else if(RegExp("^[-+*\/<>(]").test(tokens[tokens.length-1])){
      tokens.push("(");
      parenLevel++;
    }
    refreshCalcDisplay();
  }
  
  function keyPop(){
    if(parenLevel > 0 && RegExp("^[@#|)]").test(tokens[tokens.length-1])){
      tokens.push(")");
      parenLevel--;
      refreshCalcDisplay();
    }
  }
  
  function keyInput(obj){
    if(tokens.length == 1 && tokens[0] == "#0") {
      tokens[0] = "@" + obj.value;
    }
    else if(RegExp("^[-+*\/(<>]").test(tokens[tokens.length-1])){
      tokens.push("@" + obj.value);
    }
    refreshCalcDisplay();
  }
 
/********************************************************************************************
 *                                    Configure Device
 * ******************************************************************************************/
function configDevice(){
  currentBodyPush("configDevice");
  originalName = config.device.name;
  timezone = config.timezone;
  buildDevice();
  var xmlHttp = new XMLHttpRequest();
  xmlHttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var response = JSON.parse(xmlHttp.responseText);
      if(response.passwords.admin){
        EbyId("deviceName").disabled = true;
        EbyId("deviceName").title = "Passwords must be disabled to modify device name.";
      }
    }
  }
  xmlHttp.open("GET","/status?passwords=yes", true);
  xmlHttp.send(null);
}

function buildDevice(){
  var updateClasses = ["NONE", "MAJOR", "MINOR", "BETA", "ALPHA"];
  var deviceTable = EbyId("deviceTable");
  deviceTable.innerHTML = "";
  editing = false;
  originalName = config.device.name;
  
  var deviceName = addTableRow(deviceTable, "Device name:","deviceName", "input", 8);
  deviceName.value = config.device.name;
  deviceName.setAttribute("oninput","config.device.name = this.value.trim(); checkDevice();");
  deviceName.pattern = "[a-zA-Z]{1}[0-9a-zA-Z]{3,7}";

  var deviceTimezone = addTableRow(deviceTable, "Time Zone:","deviceTimezone", "input", 3);
  deviceTimezone.value = config.timezone;
  deviceTimezone.setAttribute("onchange","config.timezone = this.value; checkDevice();");
  deviceTimezone.setAttribute("style","width: 65px;");
  deviceTimezone.setAttribute("type","number");
  deviceTimezone.setAttribute("min","-12");
  deviceTimezone.setAttribute("max","13");
  deviceTimezone.setAttribute("step",".5");
  
  var allowDST = addTableRow(deviceTable, "Allow Daylight Time:","allowDST", "input");
  allowDST.setAttribute("type","checkbox");
  allowDST.checked = (config.dstrule != undefined) ? true : false;
  allowDST.setAttribute("onchange", "checkDevice();");
  
  var deviceUpdate = addTableRow(deviceTable, "Auto-update Class:","deviceUpdate", "select");
  for(i in updateClasses){
    var option = document.createElement("option");
    option.text = updateClasses[i];
    if(config.update == updateClasses[i]) option.selected = true;
    deviceUpdate.add(option);
  }
  deviceUpdate.setAttribute("onchange","config.update = this.value; checkDevice();");
}

function changeAdmin(newPassword){
  alert("Admin password will be changed to: \"" + newPassword + "\" when you save.");
}

function checkDevice(){
  editing = true;
  var complete = true;
  var nameChange = false;
  if( ! validateInput("deviceName", true)) complete = false;
  else {
    if(config.device.name != originalName){
      nameChange = true;
      var obj = EbyId("deviceName");
      obj.parentNode.appendChild(document.createElement("BR"));
      obj.parentNode.appendChild(document.createTextNode("Name changed, IoTaWatt will restart on Save."));
      obj.parentNode.appendChild(document.createElement("BR"));
      obj.parentNode.appendChild(document.createTextNode("Then restart app at " + config.device.name + ".local"));
    }
  }
  if( ! validateInput("deviceTimezone")) complete = false;
  EbyId("deviceSave").style.display = complete ? "inline" : "none";
}

function deviceCancel(){
  config.device.name = originalName;
  config.timezone = timezone;
  editing = false;
  currentBodyPop();
}

function deviceSave(){
  currentBodyPop();
  editing = false;
  config.allowdst = undefined;
  config.tzrule = undefined;
  config.dstrule = undefined;
  if(EbyId("allowDST").checked){
    for(i in dstRules){
      if(config.timezone >= dstRules[i].begOffset && config.timezone <= dstRules[i].endOffset){
        config.dstrule = dstRules[i].dstrule;
        break;
      }
    }
  }
  uploadConfig();
  if(config.device.name != originalName){
    sendRestart();
  }
}

/********************************************************************************************
 *                                    Specify Burden Resistors
 * ******************************************************************************************/

function configBurden(){
  currentBodyPush("configBurden");
  var burdenTable = EbyId("burdenTable");
  burdenTable.innerHTML = "";
  for(var i=0; i<config.device.channels; i++){
    var newRow = burdenTable.insertRow(-1);
    var newCol = newRow.insertCell(-1);
    newCol.innerHTML = "Input: " + i;
    newCol.setAttribute("align","right");
    newCol = newRow.insertCell(-1);
    var newInput = document.createElement("input");
    newInput.setAttribute("size","3");
    newInput.setAttribute("tabindex","0");
    newInput.setAttribute("onblur","newBurden(this," + i + ")");
    newInput.value = config.device.burden[i];
    newCol.appendChild(newInput);
    faults = 0;
    EbyId("burdenSave").style.display = "inline";
  }
}

function newBurden(obj, index){
  editing = true;
  var val = Number(obj.value);
  var msg = obj.nextSibling;
  if (msg != null){
    faults--;
    obj.parentNode.removeChild(obj.nextSibling);
  } 
  if(Number.isNaN(val)){
    obj.parentNode.appendChild(document.createTextNode(" Must be numeric."));
    faults++;
  }
  else if(val.toFixed(1) < 0){
    obj.parentNode.appendChild(document.createTextNode(" Must be positive."));
    faults++;
  }
  else {
    config.device.burden[index] = Number(val.toFixed(2));
    if(Number.isInteger(val)){
      config.device.burden[index] = Number(val.toFixed(0));
    }
    obj.value = config.device.burden[index];
  }
  EbyId("burdenSave").style.display = faults ? "none" : "inline";
}

function burdenCancel(){
  getConfig();
  currentBodyPop();
}  
 
function burdenSave(){
  currentBodyPop();
  for(i in config.inputs){
    if(config.inputs[i] != null && config.inputs[i].turns != undefined){
      config.inputs[i].cal = config.inputs[i].turns / config.device.burden[i];
    }
  }
  writeFile(JSON.stringify(config.device.burden), burdenFileURL, uploadConfig());
}

/********************************************************************************************
 *                                     Setup Web Server 
 * ******************************************************************************************/

function buildWebServer(){
  var servers = ["Emoncms", "InfluxDB_v1", "influxDB_v2", "PVoutput"];
  EbyId("serverHead").innerHTML = "Setup Server Uploaders";
  EbyId("divInfluxTagSet").style.display = "none";
  EbyId("divServerOutputs").style.display = "none";
  var serverTable = EbyId("serverTable");
  serverTable.innerHTML = "";
  currentBodyPush("divWebServer");
  var serverSelect = addTableRow(serverTable, "Server Type", "serverType", "select");
  serverSelect.setAttribute("onchange","editServer(this)");
  serverSelect.setAttribute("style","center");
  var option = document.createElement("option");
  option.text = "select server type";
  serverSelect.add(option);

  for(i in servers){
    var option = document.createElement("option");
    option.text = servers[i];
    option.value = servers[i];
    if(servers[i].toLowerCase() == "emoncms"){
      option.text += (config[emoncmsAlias] !== undefined) ? " (edit)" : " (add)";
    }
    if(servers[i].toLowerCase() == "influxdb_v1"){
      option.text += (config.influxdb !== undefined) ? " (edit)" : " (add)";
    }
    if(servers[i].toLowerCase() == "influxdb_v2"){
      option.text += (config.influxdb2 !== undefined) ? " (edit)" : " (add)";
    }
    if(servers[i].toLowerCase() == "pvoutput"){
      option.text += (config.pvoutput !== undefined) ? " (edit)" : " (add)";
    }
    serverSelect.add(option);
  }
  EbyId("serverDelete").style.display = "none";
  EbyId("serverSave").style.display = "none";
}

function editServer(obj){
  if(obj.value.toLowerCase() == "emoncms"){
    serverConfig = undefined;
    editEmoncms();
  }
  else if(obj.value.toLowerCase() == "influxdb_v1"){
    serverConfig = config.influxdb;
    editInfluxdb(1);
  }
  else if(obj.value.toLowerCase() == "influxdb_v2"){
    serverConfig = config.influxdb2;
    editInfluxdb(2);
  }
  else if(obj.value.toLowerCase() == "pvoutput"){
    serverConfig = undefined;
    editPVoutput();
  }
}

function serverSave(){
  editing = false;
  if(serverConfig !== undefined){
    if(serverConfig.type == "influxdb"){
      serverConfig.version = undefined;
      config.influxdb = serverConfig;
    }
    else if(serverConfig.type == "influxdb2"){
      serverConfig.version = undefined;
      config.influxdb2 = serverConfig;
    }
  }
  uploadConfig();
  currentBodyPop();
  resetDisplay();
}

function serverDelete(){
  if(serverConfig !== undefined){
    if(serverConfig.type == "influxdb"){
      config.influxdb = undefined;
    }
    else if(serverConfig.type == "influxdb2"){
      config.influxdb2 = undefined;
    }
  }
  serverSave();
}

function serverCancel(){
  editing = false;
  currentBodyPop();
  getConfig(resetDisplay);
}

function serverToggle(button){
  button.disabled = true;
  config[button.value].stop = button.innerHTML == "Stop";
  var stop;
  stop = config[button.value].stop;
  if(button.innerHTML == "Stop"){
    config[button.value].stop = true;
  }
  else {
    config[button.value].stop = undefined;
  }
  config[button.value].revision++;
  uploadConfig();
}

/***********************************************************************************************
 *                                Configure eMonCMS
 * ********************************************************************************************/
function editEmoncms(){
  editing = true;
  var serverTable = EbyId("serverTable");
  serverTable.innerHTML = "";
  
  if(config[emoncmsAlias] == undefined){
    config[emoncmsAlias] =  {type:"emoncms",
                      revision:0,
                      node:config.device.name,
                      postInterval:10,
                      bulksend:1,
                      url:"http://emoncms.org",
                      apikey:"",
                      user:"",
                      pwd:"",
                      };
    EbyId("serverDelete").style.display = "none";
    EbyId("serverHead").innerHTML = "Add Emoncms Service";
  }
  else {
    config[emoncmsAlias].revision++;
    EbyId("serverHead").innerHTML = "Edit Emoncms Service";
    EbyId("serverDelete").style.display = "inline";
    EbyId("serverDelete").setAttribute("onclick","deleteEmoncms();");
  }

  if(config[emoncmsAlias].username !== undefined){
    config[emoncmsAlias].userid = config[emoncmsAlias].username;
    config[emoncmsAlias].username = undefined;
  }
  
  if(config[emoncmsAlias].outputs === undefined){
    config[emoncmsAlias].outputs = [];
    for(var i=0; i<config.device.channels; i++){
      config[emoncmsAlias].outputs.push({name:i+1, script:"@"+i});
    }
  }
  
  var nodeInput = addTableRow(serverTable, "Node:", "serverNode", "input", 8);
  nodeInput.value = config[emoncmsAlias].node;
  nodeInput.setAttribute("onchange","config[emoncmsAlias].node = this.value; checkEmoncms();")
  nodeInput.style = "width: 80px;";
  
  var postInput =   addTableRow(serverTable, "post interval(sec): ", "serverPost", "input", 3);
  postInput.value = Number(config[emoncmsAlias].postInterval);
  postInput.setAttribute("onchange","config[emoncmsAlias].postInterval = Number(this.value); checkEmoncms();");
  postInput.setAttribute("style","width: 40px;");
  postInput.type = "number";
  postInput.min = 5;
  postInput.max = 3600;
  postInput.step = 5;
  
  if(isNaN(config[emoncmsAlias].bulksend)) config[emoncmsAlias].bulksend = 1;
  var bulksendInput = addTableRow(serverTable, "bulk send:", "serverBulk", "input", 3);
  bulksendInput.value = Number(config[emoncmsAlias].bulksend);
  bulksendInput.setAttribute("onchange","config[emoncmsAlias].bulksend = Number(this.value); checkEmoncms();");
  bulksendInput.style = "width: 40px;";
  bulksendInput.type = "number";
  bulksendInput.min = 1;
  bulksendInput.max = 60;
  bulksendInput.step = 1;
  bulksendInput.title = "Send multiple entries in a single HTTP transaction";
  
  var begdateInput = addTableRow(serverTable, "upload history from:", "serverBegd", "input", 16);
  begdateInput.setAttribute("oninput",
    "config[emoncmsAlias].begdate = new Date(this.value).getTime()/1000 - config.timezone*3600; checkEmoncms();");
  begdateInput.value =  toInputDate(config[emoncmsAlias].begdate + config.timezone*3600);
  begdateInput.type = "date";
  
  var urlInput = addTableRow(serverTable, "server URL:", "serverURL", "input", 32);
  urlInput.value = config[emoncmsAlias].url
  urlInput.setAttribute("onchange","config[emoncmsAlias].url = this.value; checkEmoncms();");
  urlInput.type = "url";
  
  var keyInput = addTableRow(serverTable, "api key:", "serverKey", "input", 32);
  keyInput.value = config[emoncmsAlias].apikey;
  keyInput.setAttribute("onchange","config[emoncmsAlias].apikey = this.value; checkEmoncms();");
  keyInput.setAttribute("type","password");
  keyInput.size = 32;
  keyInput.pattern = "[0-9a-f]{32}";
  keyInput.title = "read/write key - 32 hexadecimal digits";
  
  addTableText(serverTable,"","Specifying userid triggers secure encrypted protocol.")
  var keyInput = addTableRow(serverTable, "Emoncms userid", "serverUser", "input", 10);
  if(config[emoncmsAlias].userid === undefined) config[emoncmsAlias].userid = "";
  keyInput.value = config[emoncmsAlias].userid;
  keyInput.setAttribute("onchange","config[emoncmsAlias].userid = this.value; checkEmoncms();");
  keyInput.setAttribute("size","16");
  keyInput.pattern = "[0-9]*";
  keyInput.title = "In upper-left corner of Emoncms My Account";
  
  EbyId("divServerOutputs").style.display = "inline";
  EbyId("serverOutputsHeader").innerHTML = "Emoncms inputs";
  
  scriptEditSave = function(){
    scriptEditSet.sort(function(a, b){return a.name - b.name});
    config[emoncmsAlias].outputs = scriptEditSet;
  };
  scriptEditReturn = function(){
    scriptEditTable.innerHTML = "";
    editScript();
    checkEmoncms();
  };
  scriptEditTable = EbyId("serverOutputs");
  scriptEditSet = config[emoncmsAlias].outputs;
  scriptEditUnits = scriptEditUnitsUpload;
  scriptEditNameList = [];
  scriptEditUniqueNames = true;
  scriptEditNamePattern="^[1-9]{1}$|^[1-2]{1}[0-9]{1}$";
  scriptEditNameTitle="Name is Emoncms Input Key 1-29";
  editScript();
  checkEmoncms();
}

function checkEmoncms(){
  var complete = validateInput("serverNode") &&
                 validateInput("serverPost") &&
                 validateInput("serverBulk") &&
                 validateInput("serverURL") &&
                 validateInput("serverKey") &&
                 validateInput("serverUser") &&
                 validateInput("serverBegd") &&
                 (config[emoncmsAlias].apikey != "");
  EbyId("serverSave").style.display = complete ? "inline" : "none";
}

function deleteEmoncms(){
  config[emoncmsAlias] = undefined;
  serverSave();
}

function validateInput(id, required=false, msg=undefined){
  var obj = EbyId(id);
  while(obj.nextSibling !== null) obj.parentNode.removeChild(obj.nextSibling);
  if(obj.validationMessage != ""){
    obj.parentNode.appendChild(document.createElement("BR"));
    obj.parentNode.appendChild(document.createTextNode(msg === undefined ? obj.validationMessage : msg));
    return false;
  }
  if(required && obj.value == ""){
    obj.parentNode.appendChild(document.createElement("BR"));
    obj.parentNode.appendChild(document.createTextNode("Required"));
    return false;
  }
  return true;
}

/***********************************************************************************************
 *                                Configure influxDB
 * ********************************************************************************************/
function editInfluxdb(version){
  editing = true;
  
  var serverTable = EbyId("serverTable");
  serverTable.innerHTML = "";

  if(version == 1){
    if(serverConfig !== undefined){
      serverConfig.revision++;
      EbyId("serverHead").innerHTML = "Edit influxDB_v1 Uploader";
      EbyId("serverDelete").style.display = "inline";
      EbyId("serverDelete").setAttribute("onclick","deleteInfluxdb();");
    }
    else {
      EbyId("serverHead").innerHTML = "Add influxDB_v1 Uploader";
      serverConfig = {type:"influxdb",
                    revision: 0,
                    postInterval:10,
                    bulksend:1,
                    url:"",
                    "database": "iotawatt",
                    "measurement":"",
                    "tagset": [],
                    "outputs":[]
                    };
      EbyId("serverDelete").style.display = "none";
    }
    serverConfig.version = 1;
  }
  else { // version 2
    if(serverConfig !== undefined){
      serverConfig.revision++;
      EbyId("serverHead").innerHTML = "Edit influxDB_v2 Uploader";
      EbyId("serverDelete").style.display = "inline";
      EbyId("serverDelete").setAttribute("onclick","deleteInfluxdb();");
    }
    else {
      EbyId("serverHead").innerHTML = "Add influxDB_v2 Uploader";
      serverConfig = {type:"influxdb2",
                    revision: 0,
                    postInterval:10,
                    bulksend:6,
                    url:"",
                    "bucket": "iotawatt",
                    "token":"",
                    "orgid":"",
                    "tagset": [],
                    "outputs":[]
                    };
      EbyId("serverDelete").style.display = "none";
    }
    serverConfig.version = 2;
  }
  
  var postInput =   addTableRow(serverTable, "post interval(sec): ", "serverPost", "input", 3);
  postInput.value = Number(serverConfig.postInterval);
  postInput.setAttribute("onchange","serverConfig.postInterval = Number(this.value); checkInfluxdb();");
  postInput.setAttribute("style","width: 40px;");
  postInput.type = "number";
  postInput.setAttribute("min","5");
  postInput.setAttribute("max","3600");
  postInput.setAttribute("step","5");
  
  if(isNaN(serverConfig.bulksend)) serverConfig.bulksend = 1;
  var bulksendInput = addTableRow(serverTable, "bulk send:", "serverBulk", "input", 3);
  bulksendInput.value = Number(serverConfig.bulksend);
  bulksendInput.setAttribute("onchange","serverConfig.bulksend = Number(this.value); checkInfluxdb();");
  bulksendInput.setAttribute("style","width: 40px;");
  bulksendInput.setAttribute("type","number");
  bulksendInput.setAttribute("min","1");
  bulksendInput.setAttribute("max","10");
  bulksendInput.setAttribute("step","1");
  
  var urlInput = addTableRow(serverTable, "server URL:", "serverURL", "input", 60);
  urlInput.value = serverConfig.url
  urlInput.setAttribute("oninput","serverConfig.url = this.value; checkInfluxdb();");
  urlInput.type = "url";
  
  if(version == 1){
    var dbInput = addTableRow(serverTable, "database:", "serverDb", "input", 16);
    dbInput.value = serverConfig.database;
    dbInput.setAttribute("oninput","serverConfig.database = this.value; checkInfluxdb();");
    dbInput.placeholder = "required";
    
    var RetpInput = addTableRow(serverTable, "retention policy", "serverRetp", "input", 16);
    RetpInput.setAttribute("oninput","serverConfig.retp = this.value; checkInfluxdb();");
    if(serverConfig.retp !== undefined){
      RetpInput.value = serverConfig.retp;
    } else {
      RetpInput.value = "";
    }
    RetpInput.placeholder = "optional";
    
    if(serverConfig.user === undefined) serverConfig.user = "";
    var userInput = addTableRow(serverTable, "username:", "serverUser", "input", 16);
    userInput.value = serverConfig.user;
    userInput.setAttribute("oninput","serverConfig.user = this.value; checkInfluxdb();");
    userInput.placeholder = "optional";
  
    if(serverConfig.pwd === undefined) serverConfig.pwd = "";
    var pwdInput = addTableRow(serverTable, "password:", "serverPwd", "input", 32);
    pwdInput.value = serverConfig.pwd;
    pwdInput.setAttribute("oninput","serverConfig.pwd = this.value; checkInfluxdb();");
    pwdInput.setAttribute("type","password");
    pwdInput.placeholder = "optional";
  }
  else { // version 2
    var dbInput = addTableRow(serverTable, "Bucket:", "serverBucket", "input", 16);
    dbInput.value = serverConfig.bucket;
    dbInput.setAttribute("oninput","serverConfig.bucket = this.value; checkInfluxdb();");
    dbInput.placeholder = "required";

    if(serverConfig.orgid === undefined) serverConfig.orgid = "";
    var userInput = addTableRow(serverTable, "Organization ID:", "serverOrgID", "input", 16);
    userInput.value = serverConfig.orgid;
    userInput.setAttribute("oninput","serverConfig.orgid = this.value; checkInfluxdb();");
    userInput.placeholder = "required";
    
    if(serverConfig.authtoken === undefined) serverConfig.authtoken = "";
    var tokenInput = addTableRow(serverTable, "Authorization token:", "serverAuthToken", "input", 48);
    tokenInput.value = serverConfig.authtoken;
    tokenInput.setAttribute("oninput","serverConfig.authtoken = this.value; checkInfluxdb();");
    tokenInput.setAttribute("type","password");
    tokenInput.placeholder = "required";
  }

  var begdateInput = addTableRow(serverTable, "upload history from:", "serverBegd", "input", 16);
  begdateInput.setAttribute("oninput",
    "serverConfig.begdate = new Date(this.value).getTime()/1000 - config.timezone*3600; checkInfluxdb();");
  begdateInput.value =  toInputDate(serverConfig.begdate + config.timezone*3600);
  begdateInput.type = "date";
  
  if(serverConfig.measurement === undefined) serverConfig.measurement = "";
  var measInput = addTableRow(serverTable, "measurement", "serverMeas", "input", 16);
  measInput.setAttribute("oninput","serverConfig.measurement = this.value; checkInfluxdb();");
  measInput.value = serverConfig.measurement;
  measInput.placeholder = "default is $name";

  if(serverConfig.fieldkey === undefined) serverConfig.fieldkey = "";
  var fkeyInput = addTableRow(serverTable, "field-key", "serverFkey", "input", 16);
  fkeyInput.setAttribute("oninput","serverConfig.fieldkey = this.value; checkInfluxdb();");
  fkeyInput.value = serverConfig.fieldkey;
  fkeyInput.placeholder = "default is 'value'";
  
  EbyId("divInfluxTagSet").style.display = "inline";
  buildTagSet();
  
  EbyId("divServerOutputs").style.display = "inline";
  EbyId("serverOutputsHeader").innerHTML = "measurements";

  scriptEditSave = function(){
    serverConfig.outputs = scriptEditSet;
  };
  scriptEditReturn = function(){
    scriptEditTable.innerHTML = "";
    editScript();
    checkInfluxdb();
  };
  scriptEditTable = EbyId("serverOutputs");
  scriptEditSet = serverConfig.outputs;
  scriptEditUnits = scriptEditUnitsUpload;
  scriptEditNameList = [];
  scriptEditUniqueNames = false;
  scriptEditNameTitle="Name must be valid influx2Button field name";
  scriptEditNamePattern="^[0-9a-zA-Z_$-/\.\+]+$";
  editScript();
  checkInfluxdb();
}

function toInputDate(unixtime){
  var date = new Date(unixtime * 1000);
  var month = date.getUTCMonth()+1;
  var day = date.getUTCDate();
  return date.getUTCFullYear() + '-' + (month < 10 ? '0' : '') + month.toString() + '-' + (day < 10 ? '0' : '') + day.toString();
}

function validateRegex(id, pattern, message){
  var obj = EbyId(id);
  var match = pattern.test(obj.value);
  while(obj.nextSibling !== null) obj.parentNode.removeChild(obj.nextSibling);
  if(!match){
    obj.parentNode.appendChild(document.createElement("BR"));
    obj.parentNode.appendChild(document.createTextNode(message));
    return false;
  }
return true;
}

var pattern_URL = new RegExp('^(https?:\\/\\/)?'+ // protocol
    '((([a-z\\d]([a-z\\d-]*[a-z\\d])*)\\.)+[a-z]{2,}|'+ // domain name
    '((\\d{1,3}\\.){3}\\d{1,3}))'+ // OR ip (v4) address
    '(\\:\\d+)?(\\/[-a-z\\d%_.~+]*)*$','i');  // port and path X
var pattern_bucket = new RegExp('^[^_\\d]{1}[A-Z\\d_.\\-]{0,16}$', 'i');
var pattern_org = new RegExp('^[A-Z\\d]{16}$','i');
var pattern_token = new RegExp('^[A-Z\\d_\\-]{86}[=]{2}$','i');
var pattern_db = new RegExp('^[A-Z\\d_\\-]+$', 'i');
var pattern_retp = new RegExp('^[A-Z\\d_]*$', 'i');
var pattern_user = new RegExp('^[A-Z\\d@_]*$', 'i');
var pattern_pwd = new RegExp('^[A-Z\\d_!@#$%^&*_+?-]{0,32}$', 'i');
var pattern_meas = new RegExp('^[A-Z\\d_$-]*$', 'i');
var pattern_fkey = new RegExp('^[A-Z\\d_$-]*$', 'i');
    
function checkInfluxdb(){
  var complete = true;
  complete &= validateRegex("serverURL", pattern_URL, "Please enter a valid URL.");
  if( ! validateInput("serverPost")) complete = false;
  if( ! validateInput("serverBulk")) complete = false;
  if(serverConfig.version == 1){
    complete &= validateRegex("serverDb", pattern_db, "Alphanumeric database name required");
    complete &= validateRegex("serverRetp", pattern_retp, "Must be Alphanumeric");
    complete &= validateRegex("serverUser", pattern_user, "Must be Alphanumeric");
    complete &= validateRegex("serverPwd", pattern_pwd, "Letters, numbers and !@#$%^&*_+?-");
    if(serverConfig.retp == "") serverConfig.retp = undefined;
    if(serverConfig.user == "") serverConfig.user = undefined;
    if(serverConfig.pwd == "") serverConfig.pwd = undefined;
  }
  else {
    complete &= validateRegex("serverBucket", pattern_bucket, "Alphanumeric._- bucket name required.");
    complete &= validateRegex("serverOrgID", pattern_org, "16 character orgID required.");
    complete &= validateRegex("serverAuthToken", pattern_token, "88 character authorization token required.");
  }
  complete &= validateRegex("serverMeas", pattern_meas, "enter a string or $name, $units, if not specified $name will be used.");
  complete &= validateRegex("serverFkey", pattern_fkey, "enter a string or $name, $units. if not specified, 'value' will be used.");
  if(serverConfig.measurement == "") serverConfig.measurement = undefined;
  if(serverConfig.fieldkey == "") serverConfig.fieldkey = undefined;
  if( ! validateInput("serverBegd", false)) complete = false;
  if(serverConfig.outputs.length == 0) complete = false;
  EbyId("serverSave").style.display = complete ? "inline" : "none";
}

function deleteInfluxdb(){
  serverDelete();
}

function buildTagSet(){
  table = EbyId("influxTagSet");
  table.innerHTML = "";
  for(i in serverConfig.tagset){
    var newRow = document.createElement("tr");
    table.appendChild(newRow);
    newRow.setAttribute("class","outputsRow");
    var selColumn = document.createElement("td");
    newRow.appendChild(selColumn);
    var keyColumn = document.createElement("td");
    newRow.appendChild(keyColumn);
    var valueColumn = document.createElement("td");
    newRow.appendChild(valueColumn);
    var noteColumn = document.createElement("td");
    newRow.appendChild(noteColumn);
    var editButton = document.createElement("button");
    editButton.setAttribute("class","outputEditButton");
    editButton.appendChild(document.createTextNode("edit"));
    editButton.setAttribute("onclick","influxTagEdit(" + i + ");");
    selColumn.appendChild(editButton);
    keyColumn.innerHTML = serverConfig.tagset[i].key;
    valueColumn.innerHTML = serverConfig.tagset[i].value;
    if(i == 0){
      noteColumn.innerHTML = "(uniquely identifies this device)";
    }
  }
  var newRow = document.createElement("tr");
  table.appendChild(newRow);
  newRow.setAttribute("class","outputsRow")
  var selColumn = document.createElement("td");
  newRow.appendChild(selColumn);
  var editButton = document.createElement("button");
  editButton.setAttribute("class","outputEditButton");
  editButton.appendChild(document.createTextNode("add"));
  editButton.setAttribute("onclick","influxTagAdd(" + serverConfig.tagset.length + ")");
  selColumn.appendChild(editButton);
}

function influxTagEdit(index){
  EbyId("influxTagSave").setAttribute("onclick","influxTagSave(" + index + ");");
  EbyId("influxTagDelete").setAttribute("onclick","influxTagDelete(" + index + ");");
  EbyId("influxTagDelete").style.display = "inline";
  EbyId("influxTagSave").style.display = "inline";
  EbyId("influxTagKey").value = serverConfig.tagset[index].key;
  EbyId("influxTagKey").pattern = "^[^'\"\\s]*$";
  EbyId("influxTagValue").value = serverConfig.tagset[index].value;
  EbyId("influxTagValue").pattern = "^[^'\"\\s]*$";
  currentBodyPush("divInfluxTag");
}

function influxTagAdd(index){
  EbyId("influxTagSave").setAttribute("onclick","influxTagSave(" + index + ");");
  EbyId("influxTagDelete").style.display = "none";
  EbyId("influxTagSave").style.display = "none";
  EbyId("influxTagKey").value = "";
  EbyId("influxTagKey").pattern = "^[^'\"\\s]*$";
  EbyId("influxTagValue").value = "";
  EbyId("influxTagValue").pattern = "^[^'\"\\s]*$";
  currentBodyPush("divInfluxTag");
}

function influxTagCancel(){
  currentBodyPop();
}

function influxTagDelete(index){
  serverConfig.tagset.splice(index,1);
  buildTagSet();
  currentBodyPop();
}

function influxTagSave(index){
  if(index == serverConfig.tagset.length){
    serverConfig.tagset.push({key:EbyId("influxTagKey").value, value:EbyId("influxTagValue").value});
  }
  else {
    serverConfig.tagset[index].key = EbyId("influxTagKey").value;
    serverConfig.tagset[index].value = EbyId("influxTagValue").value;
  }
  buildTagSet();
  currentBodyPop();
}

function influxTagCheck(){
  var complete = true;
  if( ! validateInput("influxTagKey", true, "invalid string")) complete = false;
  if( ! validateInput("influxTagValue", true, "invalid string")) complete = false;
  EbyId("influxTagSave").style.display = complete ? "inline" : "none";
}


/***********************************************************************************************
 *                                Configure PVOutput
 * ********************************************************************************************/
function editPVoutput(){
  editing = true;
  
  var serverTable = EbyId("serverTable");
  serverTable.innerHTML = "";
  
  if(config.pvoutput === undefined){
    config.pvoutput = {type:"pvoutput",
                      revision: 0,
                      interval:5*60,
                      apikey:"",
                      systemid:0,
                      outputs:[]
                      };
    EbyId("serverDelete").style.display = "none";
    EbyId("serverHead").innerHTML = "Add PVOutput Service";
  }
  else {
    config.pvoutput.revision++;
    EbyId("serverHead").innerHTML = "Edit PVOutput Service";
    EbyId("serverDelete").style.display = "inline";
    EbyId("serverDelete").setAttribute("onclick","deletePVOutput();");
  }
  
  var keyInput = addTableRow(serverTable, "API Key:", "apiKey", "input", 40);
  keyInput.value = config.pvoutput.apikey;
  keyInput.setAttribute("oninput","config.pvoutput.apikey = this.value; checkPVOutput();");
  keyInput.setAttribute("size","40");
  keyInput.setAttribute("type","password");
  keyInput.pattern = "[0-9a-f]{40}";
  keyInput.title = "read/write key - 40 hexadecimal digits";
  addTableText(serverTable,"","See <a href=\"https://pvoutput.org/account.jsp\">https://pvoutput.org/account.jsp</a>")
  
  var systemInput =   addTableRow(serverTable, "System ID: ", "systemId", "input", 7);
  systemInput.value = config.pvoutput.systemid;
  systemInput.setAttribute("oninput","config.pvoutput.systemid = this.value; checkPVOutput();");
  systemInput.setAttribute("style","width: 100px;");
  systemInput.pattern = "^[1-9]{1}[0-9]*";
  systemInput.title = "System ID number - all digits";
  addTableText(serverTable,"","See <a href=\"https://pvoutput.org/addsystem.jsp\">https://pvoutput.org/addsystem.jsp</a>");
  
  var begdateInput = addTableRow(serverTable, "upload history from:", "serverBegd", "input", 16);
  begdateInput.setAttribute("oninput",
    "config.pvoutput.begdate = (this.value == '') ? undefined : new Date(this.value).getTime()/1000; checkPVOutput();");
  begdateInput.value =  toInputDate(config.pvoutput.begdate);
  begdateInput.type = "date";
  
  var reload = addTableRow(serverTable, "Reload History", "reload", "input");
  addInfo(reload,"Will ovewrite existing data, may take hours to complete.");
  reload.setAttribute("type","checkbox");
  reload.setAttribute("onchange","config.pvoutput.reload = this.checked; checkPVOutput();");
  if(config.pvoutput.reload != undefined && config.pvoutput.reload){
    reload.checked = true;
  }

  EbyId("divServerOutputs").style.display = "inline";
  EbyId("serverOutputsHeader").innerHTML = "Status Outputs";

  
  scriptEditSave = function(){
    config.pvoutput.outputs = scriptEditSet;
  };
  scriptEditReturn = function(){
    scriptEditTable.innerHTML = "";
    editScript();
    checkPVOutput();
  };
  scriptEditTable = EbyId("serverOutputs");
  scriptEditSet = config.pvoutput.outputs;
  scriptEditUnits = scriptEditUnitsUpload;
  scriptEditNameList = ["generation", "consumption", "voltage", "extended_1(v7)", "extended_2(v8)", "extended_3(v9)", "extended_4(v10)", "extended_5(v11)", "extended_6(v12)"];
  scriptEditUniqueNames = true;
  scriptEditNamePattern = undefined;
  scriptEditNameTitle = "extended is used for donator mode only";
  editScript();
  checkPVOutput();
}

function checkPVOutput(){
  var complete = false;
  for(i in config.pvoutput.outputs){
    if(config.pvoutput.outputs[i].name == "consumption" || config.pvoutput.outputs[i].name == "generation"){
      complete = true;
    }
  }
  if( ! validateInput("apiKey")) complete = false;
  if( ! validateInput("systemId", true)) complete = false;
  if( ! validateInput("serverBegd", false)) complete = false;
  EbyId("serverSave").style.display = complete ? "inline" : "none";
  if(config.pvoutput.reload != undefined && config.pvoutput.reload == false){
    config.pvoutput.reload = undefined;
  }
}
  
function deletePVOutput(){
  config.pvoutput = undefined;
  serverSave();
}

function PVoutToggle(){
  if(config.pvoutput.stop === undefined || config.pvoutput.stop == false){
    config.pvoutput.stop = true;
  }
  else {
    config.pvoutput.stop = undefined;
  }
  config.pvoutput.revision++;
  uploadConfig();
}

/***********************************************************************************************
 *                        Setup and run status display
 * *******************************************************************************************/
function statusBegin(){
  EbyId("tabWifi").style.display = "none";
  // EbyId("statusInflux1Div").style.display = config.influxdb !== undefined ? "inline" : "none";
  // EbyId("statusInflux2Div").style.display = config.influxdb2 !== undefined ? "inline" : "none";
  // EbyId("statusPVoutDiv").style.display = config.pvoutput !== undefined ? "inline" : "none";
  // EbyId("statusEmoncmsDiv").style.display = (config[emoncmsAlias] !== undefined) ? "inline" : "none";
  currentBodyPush("divStatus");
  getStatus = true;
  statusGet();
}

function statusGet(){
  var xmlHttp = new XMLHttpRequest();
  xmlHttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      statusDisplay(xmlHttp.responseText);
    }
  }
  xmlHttp.open("GET","/status?state&inputs&outputs&stats&wifi&datalogs&influx1&influx2&emoncms&pvoutput", true);
  xmlHttp.send(null);
}

function statusDisplay(statusMessage){
  var status = JSON.parse(statusMessage);
  var statusTable = EbyId("statusTableL");
  statusTable.innerHTML = null;
  addRow();
  column3.appendChild(document.createTextNode("Firmware version: " + status.stats.version));
  addRow();
  column3.appendChild(document.createTextNode("Running time: " + formatRunTime(status.stats.runseconds)));
  addRow();
  column3.appendChild(document.createTextNode("free Heap: " + status.stats.stack));
  if(status.stats.lowbat !== undefined && status.stats.lowbat){
    addRow();
    column3.appendChild(document.createTextNode("RTC battery is low."));
  }
  
  statusTable = EbyId("statusTableR");
  statusTable.innerHTML = null;
  addRow();
  column3.appendChild(document.createTextNode(status.stats.cyclerate.toFixed(0) + " samples per AC cycle"));
  addRow();
  column3.appendChild(document.createTextNode(status.stats.chanrate.toFixed(1) + " AC cycles sampled/second"));
  addRow();
  column3.appendChild(document.createTextNode(status.stats.frequency.toFixed(1) + " Hz"));
  
  if(status.wifi !== undefined && status.wifi.connecttime > 0){
    EbyId("tabWifi").style.display = "inline";
      wifiStatus = EbyId("tableWifi");
      wifiStatus.innerHTML = "<tr><th width=\"50%\"></th><th width=\"50%\"></th></tr>";
      wifiStatus.innerHTML += "<tr><td>SSID: " + status.wifi.SSID + "</td><td>IP: " + status.wifi.IP + "</td></tr>";
      wifiStatus.innerHTML += "<tr><td>channel: " + status.wifi.channel + "</td><td>RSSI: " + status.wifi.RSSI + "</td></tr>";
      wifiStatus.innerHTML += "<tr><td>MAC: " + status.wifi.mac + "</td><td>connected: " + formatRunTime(status.stats.currenttime - status.wifi.connecttime) + "</td></tr>";
  }
  
  if(status.datalogs !== undefined){
    var logtab = EbyId("statusLogsTable");
    logtab.innerHTML = null;
    for(i in status.datalogs){
      var units = ["", "KB", "MB", "GB"];
      var size = status.datalogs[i].size;
      var u = 0;
      while(size > 1024){
        size /=1024;
        u++;
      }
      logtab.innerHTML += "<tr><td>" + status.datalogs[i].id + " Log:</td>" +
                         "<td>" + fixedDateTime(status.datalogs[i].firstkey) + " - " + fixedDateTime(status.datalogs[i].lastkey) + "</td>" +
                         "<td>" + size.toPrecision(4) + " " + units[u] + "</td></tr>";
    }
  }
    
  var influx1Div = EbyId("statusInflux1Div");
  if(status.influx1 !== undefined && status.influx1.state != "not running"){
    show("statusInflux1Div", "block");
    show("tabUploaders");
    var button = EbyId("influx1Button");
    button.disabled = false;
    if(status.influx1.status == "running"){
      button.innerHTML = "Stop";
      EbyId("influx1text").innerHTML = "Running"
    }
    else {
      button.innerHTML = "Start";
      EbyId("influx1text").innerHTML = "Stopped"
    }
    EbyId("influx1text").innerHTML += ", Last update " + formatDateTime(status.influx1.lastpost);
    EbyId("influx1msg").innerHTML = "";
    if(status.influx1.message){
      EbyId("influx1msg").innerHTML = "<br>" + status.influx1.message;
    }
  } 
  else {
    hide("statusInflux1Div");
  }
  
  var influx2Div = EbyId("statusInflux2Div");
  if(status.influx2 !== undefined && status.influx2.state != "not running"){
    show("statusInflux2Div", "block");
    show("tabUploaders");
    
    var button = EbyId("influx2Button");
    button.disabled = false;
    if(status.influx2.status == "running"){
      button.innerHTML = "Stop";
      EbyId("influx2text").innerHTML = "Running"
    }
    else {
      button.innerHTML = "Start";
      EbyId("influx2text").innerHTML = "Stopped"
    }
    EbyId("influx2text").innerHTML += ", Last update " + formatDateTime(status.influx2.lastpost);
    EbyId("influx2msg").innerHTML = "";
    if(status.influx2.message){
      EbyId("influx2msg").innerHTML = "<br>" + status.influx2.message;
    }
  }
  else {
    hide("statusInflux2Div");
  }
  
  var EmoncmsDiv = EbyId("statusEmoncmsDiv");
  if(status.emoncms !== undefined && status.emoncms.state != "not running"){
    show("statusEmoncmsDiv", "block");
    show("tabUploaders");
    var button = EbyId("EmoncmsButton");
    button.disabled = false;
    if(status.emoncms.status == "running"){
      button.innerHTML = "Stop";
      EbyId("Emoncmstext").innerHTML = "Running"
    }
    else {
      button.innerHTML = "Start";
      EbyId("Emoncmstext").innerHTML = "Stopped"
    }
    EbyId("Emoncmstext").innerHTML += ", Last update " + formatDateTime(status.emoncms.lastpost);
    EbyId("Emoncmsmsg").innerHTML = "";
    if(status.emoncms.message){
      EbyId("Emoncmsmsg").innerHTML = "<br>" + status.emoncms.message;
    }
  }
  else {
    hide("statusEmoncmsDiv");
  }
  
  var PVoutDiv = EbyId("statusPVoutDiv");
  if(status.pvoutput !== undefined && status.pvoutput.state != "not running"){
    show("statusPVoutDiv", "block");
    show("tabUploaders");
    var button = EbyId("PVoutputButton");
    button.disabled = false;
    if(status.pvoutput.status == "running"){
      button.innerHTML = "Stop";
      EbyId("PVoutputtext").innerHTML = "Running"
    }
    else {
      button.innerHTML = "Start";
      EbyId("PVoutputtext").innerHTML = "Stopped"
    }
    EbyId("PVoutputtext").innerHTML += ", Last update " + formatDateTime(status.pvoutput.lastpost);
    EbyId("PVoutputmsg").innerHTML = "";
    if(status.pvoutput.message){
      EbyId("PVoutputmsg").innerHTML = "<br>" + status.pvoutput.message;
    }
  }
  else {
    hide("statusPVoutDiv");
  }
   
  
  statusTable = EbyId("inputStatusTable");
  statusTable.innerHTML = "";
  for(i in status.inputs){
    addRow();
    
    for(j in config.inputs){
      if(config.inputs[j] !== null && config.inputs[j].channel == status.inputs[i].channel){
        if (status.inputs[i].reversed)  column1.innerHTML += "<span title=\"CT reversed\">&#8634 </span>";
        if(config.inputs[j].name !== undefined) {
          column1.innerHTML += "<strong>" + config.inputs[j].name + ":</strong>";
        }
        break;
      }
    }
    
    
    if(status.inputs[i].Watts !== undefined){
      var wattNode = document.createElement("font");
      wattNode.innerHTML = status.inputs[i].Watts + "&nbsp;" + "Watts";
      column3.appendChild(wattNode);
      
      if(status.inputs[i].reversed == "true"){
        //wattNode.setAttribute("color","DarkRed");
      } 
      if(status.inputs[i].Watts < 0){
          wattNode.setAttribute("color","DarkGreen");
      }
      if(Math.abs(status.inputs[i].Watts) >= 60){
        var pf = Math.abs(status.inputs[i].Pf);
        var pfNode = document.createElement("font");
        var PF2 = pf.toFixed(2);
        pfNode.innerHTML = ", pf" + "&nbsp;" + ((PF2 < 1) ? PF2.substr(1) : PF2);
        column3.appendChild(pfNode);
      }
      
    }
    else if(status.inputs[i].Vrms !== undefined){
      column3.appendChild(document.createTextNode(status.inputs[i].Vrms.toFixed(1) + " Volts"));
    }
  }
  
  statusTable = EbyId("outputStatusTable");
  statusTable.innerHTML = "";
  for(i in status.outputs){
    addRow();
    column1.innerHTML += "<strong>" + status.outputs[i].name + ":</strong>";
    var wattNode = document.createElement("font");
    wattNode.innerHTML = status.outputs[i].value.toFixed(unitsPrecision(status.outputs[i].units)) + " " + status.outputs[i].units;
    column3.appendChild(wattNode);
  }
    
  setTimeout(function(){if(getStatus)statusGet();},1000);
  
  function addRow(){
    newRow = document.createElement("tr");
    statusTable.appendChild(newRow);
    column1 = document.createElement("td");
    column1.setAttribute("align","right");
    newRow.appendChild(column1);
    column2 = document.createElement("td");
    newRow.appendChild(column2);
    column3 = document.createElement("td");
    newRow.appendChild(column3);
  }
}

function unitsPrecision(unitArg){
  for(u in units){
    if(units[u].unit == unitArg) return units[u].dp;
  }
  return 0;
}

function formatRunTime(time){
  var days = Math.floor(time / 86400);
  time -= days * 86400;
  var hours = Math.floor(time / 3600);
  time -= hours * 3600;
  var minutes = Math.floor(time / 60);
  time -= minutes * 60;
  var seconds = parseInt(time % 60, 10);
  return (days > 0 ? days + "d  " : "") +  hours + "h " + minutes + "m " + seconds + "s";
}

function formatDateTime(unixtime){
  if(unixtime == 0) return ""; 
  var date = new Date(unixtime*1000);
  return date.toLocaleDateString() + " " + date.toLocaleTimeString();
}

function fixedDateTime(unixtime){
  if(unixtime == 0) return ""; 
  var date = new Date(unixtime*1000);
  return twoDigit(date.getMonth()+1) + "/" + twoDigit(date.getDate()) + "/" + date.getFullYear() + " "
          + twoDigit(date.getHours()) + ":" +   twoDigit(date.getMinutes()) + ":" + twoDigit(date.getSeconds());
}

function twoDigit(i){
  return ((i < 10) ? "0" : "") + i.toFixed(0); 
}
  
function checkConfig(){
  var rewrite = false;
  readFile(burdenFileURL, function(response){
    config.device.burden = JSON.parse(response);
    }); 
  if(config.device.channels === undefined){
    config.device.channels = 15;
    rewrite = true;
  }
  if(config.inputs === undefined){
    config.inputs = [{channel:0, type:"VT", model:"generic", cal:10, phase:2}];
    rewrite = true;
  }
  for(var i=0; i<config.inputs.length; i++){
    if(config.inputs[i] === undefined ||
    (config.inputs[i] !== null && config.inputs[i].channel > i)){
      config.inputs.splice(i,0,null);
      rewrite = true;
    }
  }
  for(var i=config.inputs.length; i<config.device.channels; i++){
    config.inputs.push(null);
    rewrite = true;
  }
  config.inputs.splice(config.device.channels,config.inputs.length-config.device.channels);
  
  if(config.device.burden === undefined){
    config.device.burden = [0];
    rewrite = true;
  } 
  for(var i=config.device.burden.length; i<config.device.channels; i++){
    config.device.burden.push(24);
  }
  for(i in config.inputs){
    if(config.inputs[i] != null && config.inputs[i].model != "generic"){
      var table = tables.CT;
      if(config.inputs[i].type == "VT") table = tables.VT;
      for(j in table){
        if(config.inputs[i].model == table[j].model){
          config.inputs[i].phase = table[j].phase;
        }
      }
    }
  }
  
  if(config.format == 1){
    for(i in config.outputs) {
      config.outputs[i].script = old2newScript(config.outputs[i].script);
    }
    config.format == 2;
    rewrite = true;
  }
  
  if(config.server != undefined){
    if(config.server.type == "emoncms"){
      config.emoncms = config.server;
    }
    config.server = undefined;
    rewrite = true;
  }
  
  if(rewrite){
    uploadConfig();
  }
 }

function old2newScript(oldScript){
  var newScript = "";
  for(i in oldScript){
    if(oldScript[i].oper == "const"){
      newScript += "#" + oldScript[i].value;
    }
    else if(oldScript[i].oper == "input"){
      newScript += "@" + oldScript[i].value;
    }
    else if(oldScript[i].oper == "binop"){
      newScript += oldScript[i].value; 
    } 
    else if(oldScript[i].oper == "push"){
      newScript += "(";
    }
    else if(oldScript[i].oper == "pop"){
      newScript += ")";
    }
    else if(oldScript[i].oper == "abs"){
      newScript += "|";
    }   
  }
  return newScript;
}

/********************************************************************************************
 *                       Main menu navigation.
 * *****************************************************************************************/
function mainMenuButton(obj, handler){
  // only require canceling form if we've made changes to it
  if (
   !(
      (editing || editingScript) &&
      didFormChange() &&
      !confirm('You have unsaved changes. Are you sure you want to leave without saving?')
    )
  )
    pageChange(handler);
}

function pageChange(handler)
{
  currentBodyPop();
  resetDisplay();
  if (handler)
    handler();
}

function didFormChange() {
  var hasChanges = false;

  document.querySelectorAll("input:not(button):not([type=hidden])").forEach(
    function (item) {
      if (
          (item.type == "text" || item.type == "textarea" || item.type == "hidden") &&
          item.defaultValue != item.value) {
        hasChanges = true;
        return false;
      }
      else {
        if ((item.type == "radio" || item.type == "checkbox") && item.defaultChecked != item.checked) {
          hasChanges = true;
          return false;
        }
        else {
          if ((item.type == "select-one" || item.type == "select-multiple")) {
            for (var x = 0; x < item.length; x++) {
              if (item.options[x].selected != item.options[x].defaultSelected) {
                hasChanges = true;
                return false;
              }
            }
          }
        }
      }
    }
  );
  return hasChanges;
}

function storeForm() {
  document.querySelectorAll("input:not(button):not([type=hidden])").forEach(
    function (item) {
      if (item.type == "text" || item.type == "textarea" || item.type == "hidden") {
        item.defaultValue = item.value;
      }
      if (item.type == "radio" || item.type == "checkbox") {
        item.defaultChecked = item.checked;
      }
      if (item.type == "select-one" || item.type == "select-multiple") {
        for (var x = 0; x < item.length; x++) {
          item.options[x].defaultSelecvalueted = item.options[x].selected
        }
      }
    }
  );
}

function currentBodyPop(){
  if(currentBody.length > 0) EbyId(currentBody.pop()).style.display = "none";
  if(currentBody.length > 0) EbyId(currentBody[currentBody.length-1]).style.display = "block";
  else resetDisplay();
}

function currentBodyPush(newBody){
  if(currentBody.length > 0){
    if(currentBody[currentBody.length-1] == newBody) return;
    if(currentBody[currentBody.length-1].search("Menu") == -1) {
      EbyId(currentBody[currentBody.length-1]).style.display = "none";
    }
  } 
  currentBody.push(newBody);
  EbyId(currentBody[currentBody.length-1]).style.display = "table";
  EbyId("mainBody").style.display = "block";
}

function resetDisplay(){
  getStatus = false;
  editing = false;
  while(currentBody.length > 0) currentBodyPop();
  hide("mainBody");
  storeForm(); // save form data to compare later
}

function loadGraph(){
  var myWindow = window.open(graphURL, "_self");
  resetDisplay();
}

function loadGraph2(){
  var myWindow = window.open(graph2URL, "_self");
  resetDisplay();
}

function loadEdit(){
  resetDisplay();
  var myWindow = window.open(editURL, "_self");
}

function showMsgs(){
  resetDisplay();
  var myWindow = window.open(msgsFileURL, "_self");
}

function toggleDisplay(id){
  var element = EbyId(id);
  if(! editing){
    element.style.display = element.style.display == "none" ? "block" : "none";
  }
}

function show(elementID, method = "inline"){
  EbyId(elementID).style.display = method;
}

function hide(elementID){
  EbyId(elementID).style.display = "none";
}

/**********************************************************************************************
 *              File I/O and management
 * *******************************************************************************************/

function getConfig(callback){
  EbyId("panicMessageDiv").style.display = "none";
  readFile(configFileURL, function(response){
                            if(response == undefined){
                              panic("configuration not found.");
                            }
                            else {
                              try {
                                config = JSON.parse(response);
                              }
                              catch (e) {
                                panic("configuration parse failed: " + e.message);
                                return;
                              }
                              noConfig = false;
                              checkConfig();
                              document.title = config.device.name;
                              EbyId("heading").innerHTML = config.device.name + " Power Monitor";
                              if(callback !== undefined) callback();
                              EbyId("mainMenu").style.display = "block";
                            }
                          })
}

function panic(message){
  EbyId("panicList").innerHTML = "<p>" + message + "</p>";
  EbyId("panicMessageDiv").style.display = "block";
  noConfig = true;
}

function getTables(){
  readFile(configTablesURL, function(response){tables = JSON.parse(response);});
}

function readFile(path, responseHandler){
  var xmlHttp = new XMLHttpRequest();
  xmlHttp.onreadystatechange = function() {
    if (this.readyState == 4) {
      if(this.status == 200){
        if(this.getResponseHeader("X-configSHA256") !== null){
          configSHA256 = this.getResponseHeader("X-configSHA256");
        }
        responseHandler(this.responseText);
      } 
      else {
        responseHandler(undefined);
      }
    }
  };
  xmlHttp.open("GET", path, true);
  xmlHttp.send();
}

function uploadConfig(){
  if(noConfig) return;
  writeFile(JSON.stringify(config, null, "\t"), configNewURL);
}

function writeFile(fileString, url, responseHandler){
  var xmlHttp = new XMLHttpRequest();
  xmlHttp.onreadystatechange = function() {
    if (this.readyState == 4) {
      if(this.status == 200) {
        if(this.getResponseHeader("X-configSHA256") !== null){
          configSHA256 = this.getResponseHeader("X-configSHA256");
        }
        if(responseHandler !== undefined){
          responseHandler();
        }
      }
      else if(this.status == 409) {
        alert("config file not current, operation aborted,\nconfig file not updated, restarting app.");
        setup();
      }
    }
  };
  
  var formData = new FormData();
  var blob = new Blob([fileString], {type: 'plain/text'},  url);
  formData.append("file", blob, url);
  var URI = "/edit";
  xmlHttp.open("POST", URI);
  if(url == configFileURL){
    xmlHttp.setRequestHeader("X-configSHA256", configSHA256);
  }
  xmlHttp.send(formData);
}


function setup(){
    getTables();
    getConfig();
    resetDisplay();
}