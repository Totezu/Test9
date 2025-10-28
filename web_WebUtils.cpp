#include "web_WebInternals.h"
#include <math.h>
#include <stdlib.h>

// ---------- HTTP ----------
void sendHeader(EthernetClient& c, const char* ct) {
  c.println(F("HTTP/1.1 200 OK"));
  c.print(F("Content-Type: ")); c.println(ct);
  c.println(F("Cache-Control: no-store, no-cache, must-revalidate"));
  c.println(F("Pragma: no-cache"));
  c.println(F("Connection: close"));
  c.println();
}

// ---------- String / numbers ----------
bool strEq(const String& a, const char* b) { return a.equalsIgnoreCase(b); }

int toIntSafe(const String& s, int def) {
  char* endp = nullptr;
  long v = strtol(s.c_str(), &endp, 10);
  return (endp && *endp=='\0') ? (int)v : def;
}
unsigned long toULongSafe(const String& s, unsigned long def) {
  char* endp = nullptr;
  unsigned long v = strtoul(s.c_str(), &endp, 10);
  return (endp && *endp=='\0') ? v : def;
}
double toDoubleSafe(const String& s, double def) {
  char* endp = nullptr;
  double v = strtod(s.c_str(), &endp);
  return (endp && *endp=='\0') ? v : def;
}

// ---------- URL decode ----------
String urlDecode(const String& in) {
  String out; out.reserve(in.length());
  for (size_t i=0;i<in.length();++i) {
    char c=in[i];
    if (c=='+') out+=' ';
    else if (c=='%' && i+2<in.length()) {
      auto hex=[&](char h)->int{
        if (h>='0'&&h<='9') return h-'0';
        if (h>='a'&&h<='f') return 10+(h-'a');
        if (h>='A'&&h<='F') return 10+(h-'A');
        return -1;
      };
      int v1=hex(in[i+1]), v2=hex(in[i+2]);
      if (v1>=0 && v2>=0) { out += char((v1<<4)|v2); i+=2; }
      else out += c;
    } else out+=c;
  }
  return out;
}

// ---------- IP helpers ----------
bool parseIP(const String& s, IPAddress& out) {
  int parts[4]={0,0,0,0}; int idx=0; int start=0;
  while (start < (int)s.length() && idx<4) {
    int dot = s.indexOf('.', start); if (dot<0) dot = s.length();
    String seg = s.substring(start, dot); seg.trim();
    if (seg.length()==0) return false;
    int v = toIntSafe(seg, -1);
    if (v<0 || v>255) return false;
    parts[idx++] = v;
    start = dot + 1;
  }
  if (idx != 4) return false;
  out = IPAddress((uint8_t)parts[0],(uint8_t)parts[1],(uint8_t)parts[2],(uint8_t)parts[3]);
  return true;
}
String ipToString(const IPAddress& ip) {
  String s; s.reserve(16);
  s += String(ip[0]); s += '.'; s += String(ip[1]); s += '.';
  s += String(ip[2]); s += '.'; s += String(ip[3]);
  return s;
}

// ---------- JSON helpers ----------
void printBoolJSON(EthernetClient& c, bool v) {
  c.print(v ? F("true") : F("false"));
}
void printDoubleJSON(EthernetClient& c, double v, uint8_t prec) {
  if (!isfinite(v)) { c.print(F("null")); return; }
  char buf[24];
  dtostrf(v, 0, prec, buf);
  c.print(buf);
}