#include "IotaOutputChannel.h"

bool IotaOutputChannel::setScript(String script){
  IS.encodeScript(script);
  //IS.printScript();
}

double IotaOutputChannel::runScript(double inputCallback(int)){
  return IS.runScript(inputCallback);
}

bool IotaScript::encodeScript(String script){
  int tokenCount = 0;
  int constCount = 0;
  delete[] _tokens;
  delete[] _constants;
  for(int i=0; i<script.length(); i++){
    if(script[i] == '#')constCount++;
    if((!isDigit(script[i])) && (script[i] != '.')) tokenCount++;
  }
  _tokens = new IStoken[tokenCount + 1];
  _constants = new float[constCount];

  int j = 0;      
  for(int i=0; i<tokenCount; i++){
    if(script[j] == '@'){
      int s = ++j;
      while(isDigit(script[++j])); 
      _tokens[i] = getInputOp + script.substring(s,j).toInt();
    } 
    else if (script[j] == '#'){
      _tokens[i] = getConstOp + --constCount;
      int s = ++j;
      while(isDigit(script[++j]) || script[j] == '.');
      _constants[constCount] = script.substring(s,j).toFloat();
    }
    else {
      _tokens[i] = opChars.indexOf(script[j++]);
    }      
  }
  _tokens[tokenCount] = 0;
}

double IotaScript::runScript(double inputCallback(int)){
  IStoken* tokens = _tokens;
  return recursiveRunScript(&tokens, inputCallback);
}

double IotaScript::recursiveRunScript(IStoken** tokens, double inputCallback(int)){
  double result = 0.0;
  double operand = 0.0;
  IStoken pendingOp = opAdd;
  IStoken* token = *tokens;
  do {
    // Serial.print(result);
    // Serial.print(", ");
    // Serial.print(pendingOp);
    // Serial.print(", ");
    // Serial.println(operand);
    if(*token >= opAdd && *token <= opDiv){
      result = evaluate(result, pendingOp, operand);
      pendingOp = *token;
      operand = 0;
      if(*token == opDiv || *token == opMult) operand = 1;
    }
    else if(*token == opAbs){
      if(operand < 0) operand *= -1;
    }				
    else if(*token == opPush){
      token++;
      operand = recursiveRunScript(&token, inputCallback);
    }
    else if(*token == opPop){ 
      *tokens = token;
      return evaluate(result, pendingOp, operand);
    }
    else if(*token == opEq){
      return evaluate(result, pendingOp, operand);
    }
  
    if(*token & getInputOp){
      operand = inputCallback(*token % 32);
    }
    if(*token & getConstOp){
      operand = _constants[*token % 32];
    }
  } while(token++);
}

double IotaScript::evaluate(double result, IStoken token, double operand){
  // Serial.print("evaluate: ");
  // Serial.print(result);
  // Serial.print(", ");
  // Serial.print(token);
  // Serial.print(", ");
  // Serial.println(operand);
  switch (token) {
    case opAdd:
      return result + operand;
    case opSub:
      return result - operand;
    case opMult:
      return result * operand;
    case opDiv:
      return result / operand;		
  }
}

void IotaScript::printScript(){
  IStoken* token = _tokens;
  String msg = "";
  while(*token){
    if(*token < getInputOp){
      msg += String(opChars[*token]) + " ";
    }
    else if(*token & getInputOp){
      msg += "@" + String(*token - getInputOp)+ " ";
    }
    else if(*token & getConstOp){
      msg += String(_constants[*token - getConstOp]) + " ";
    }
    else {
      msg += "token(" + String(*token) + " ";
    }
    token++;
  }
  Serial.println(msg);
}
