#include <Arduino.h>

#include "IotaScript.h"

Script *Script::next() { return _next; }

char *Script::name() { return _name; }

char *Script::units() { return _units; }

size_t ScriptSet::count() { return _count; }

Script *ScriptSet::first() { return _listHead; }

void Script::print()
{
  uint8_t *token = _tokens;
  String string = "Script:";
  string += _name;
  string += ",units:";
  string += _units;
  string += ' ';
  while (*token)
  {
    if (*token < getInputOp)
    {
      string += String(opChars[*token]);
    }
    else if (*token & getInputOp)
    {
      string += "@" + String(*token - getInputOp);
    }
    else if (*token & getConstOp)
    {
      string += String(_constants[*token - getConstOp], 4);
      while (string.endsWith("0"))
        string.remove(string.length() - 1);
      if (string.endsWith("."))
        string += '0';
    }
    else
    {
      string += "token(" + String(*token) + ")";
    }
    token++;
  }
  Serial.println(string);
}

bool Script::encodeScript(const char *script)
{
  int tokenCount = 0;
  int constCount = 0;
  int consts = constCount;
  for (int i = 0; i < strlen(script); i++)
  {
    if (script[i] == '#')
      constCount++;
    if ((!isDigit(script[i])) && (script[i] != '.'))
      tokenCount++;
  }
  _tokens = new uint8_t[tokenCount + 1];
  _constants = new float[constCount];
  int j = 0;
  for (int i = 0; i < tokenCount; i++)
  {
    if (script[j] == '@')
    {
      int n = script[++j] - '0';
      while (isDigit(script[++j]))
        n = n * 10 + (script[j] - '0');
      _tokens[i] = getInputOp + n;
    }
    else if (script[j] == '#')
    {
      _tokens[i] = getConstOp + --constCount;
      String number;
      while (isDigit(script[++j]) || script[j] == '.')
        number += script[j];
      _constants[constCount] = number.toFloat();
    }
    else
    {
      for (int k = 0; k < strlen(opChars); ++k)
      {
        if (script[j] == opChars[k])
        {
          _tokens[i] = k;
          break;
        }
      }
      j++;
    }
  }
  _tokens[tokenCount] = 0;
}

double Script::run(double inputCallback(int))
{
  uint8_t *tokens = _tokens;
  return runRecursive(&tokens, inputCallback);
}

double Script::runRecursive(uint8_t **tokens, double inputCallback(int))
{
  double result = 0.0;
  double operand = 0.0;
  uint8_t pendingOp = opAdd;
  uint8_t *token = *tokens;
  do
  {
    if (*token >= opAdd && *token <= opDiv)
    {
      result = evaluate(result, pendingOp, operand);
      pendingOp = *token;
      operand = 0;
      if (*token == opDiv || *token == opMult)
        operand = 1;
    }
    else if (*token == opAbs)
    {
      if (operand < 0)
        operand *= -1;
    }
    else if (*token == opPush)
    {
      token++;
      operand = runRecursive(&token, inputCallback);
    }
    else if (*token == opPop)
    {
      *tokens = token;
      return evaluate(result, pendingOp, operand);
    }
    else if (*token == opEq)
    {
      return evaluate(result, pendingOp, operand);
    }

    if (*token & getInputOp)
    {
      operand = inputCallback(*token % 32);
    }
    if (*token & getConstOp)
    {
      operand = _constants[*token % 32];
    }
  } while (token++);
}

double Script::evaluate(double result, uint8_t token, double operand)
{
  switch (token)
  {
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
