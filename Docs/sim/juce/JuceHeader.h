// 監査用の最小 JUCE スタブ (構文チェック専用)
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <array>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <atomic>
namespace juce {
template<class T> constexpr T jlimit(T lo, T hi, T v){ return v<lo?lo:(v>hi?hi:v); }
template<class T> constexpr T jmin(T a, T b){ return a<b?a:b; }
template<class T> constexpr T jmax(T a, T b){ return a>b?a:b; }
template<class... A> void ignoreUnused(A&&...) {}
template<class T> struct MathConstants { static constexpr T pi = (T)3.14159265358979323846;
  static constexpr T twoPi = (T)6.28318530717958647692; static constexpr T halfPi = (T)1.57079632679489661923; };
inline int roundToInt(double v){ return (int)std::lround(v); }
struct Random { float nextFloat(){ s=s*1664525u+1013904223u; return (float)(s>>8)/16777216.0f; }
  int nextInt(int n){ return (int)(nextFloat()*(float)n); } unsigned s=12345u; };
struct String { String(){} String(const char*){} String(int){} String(float,int=2){}
  String(double,int=2){} String operator+(const String&) const { return {}; }
  bool isNotEmpty() const { return false; } };
inline String operator+(const char*, const String&){ return {}; }
struct StringArray { StringArray(){} StringArray(std::initializer_list<String>){}
  int size() const { return 0; } String operator[](int) const { return {}; } };
struct MidiMessage { bool isNoteOn() const {return false;} bool isNoteOff() const {return false;}
  bool isControllerOfType(int) const {return false;} int getNoteNumber() const {return 60;}
  float getFloatVelocity() const {return 1.0f;} int getControllerValue() const {return 0;} };
struct MidiMeta { MidiMessage getMessage() const { return {}; } };
struct MidiBuffer { const MidiMeta* begin() const {return nullptr;} const MidiMeta* end() const {return nullptr;} };
template<class T> struct LinearSmoothedValue { void reset(double,double){} void setTargetValue(T){}
  void setCurrentAndTargetValue(T){} T getNextValue(){return {};} T getTargetValue() const {return {};} };
}
