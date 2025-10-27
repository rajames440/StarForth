/*
                                  ***   StarForth   ***

  RuntimeStatistics.cpp- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:04.600-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/vampire-4.8/src/Debug/RuntimeStatistics.cpp
 */
/**
 * @file RuntimeStatistics.cpp
 * Implements class RuntimeStatistics.
 */

#include "Lib/Comparison.hpp"
#include "Lib/Int.hpp"
#include "Lib/Sort.hpp"
#include "Lib/Stack.hpp"

#include "RuntimeStatistics.hpp"

#if RUNTIME_STATS

namespace Debug
{

void RSMultiCounter::print(ostream& out)
{
  out << name() << ":"<< endl;
  for(size_t i=0;i<_counters.size();i++) {
    if(_counters[i]) {
      out << "  " << i << ": " << _counters[i] <<endl;
    }
  }
}

RSMultiStatistic::~RSMultiStatistic()
{
  for(size_t i=0;i<_values.size();i++) {
    ValList::destroy(_values[i]);
  }
}

void RSMultiStatistic::print(ostream& out)
{
  out << name() << ":"<< endl;
  for(size_t i=0;i<_values.size();i++) {
    if(_values[i]) {

      ValList* vals=_values[i];

      ValList::Iterator vit(vals);
      size_t cnt=0;
      long long int sum=0;
      int min=vals->head();
      int max=vals->head();
      while(vit.hasNext()) {
	int val=vit.next();
	cnt++;
	sum+=val;
	if(val<min) {
	  min=val;
	}
	if(val>max) {
	  max=val;
	}
      }
      
      out << "  " << i << ": " << 
              "cnt: "+Int::toString(cnt)+
              ", avg: "+Int::toString(static_cast<float>(sum)/cnt)+
              ", min: "+Int::toString(min)+
              ", max: "+Int::toString(max) << endl;
    }
  }
}


RuntimeStatistics* RuntimeStatistics::instance()
{
  static RuntimeStatistics inst;

  return &inst;
}

RuntimeStatistics::RuntimeStatistics()
{
}

RuntimeStatistics::~RuntimeStatistics()
{
  while (_objs.isNonEmpty()) {
    delete _objs.pop();
  }
}

void RuntimeStatistics::print(ostream& out)
{
  out<<"----  Runtime statistics ----"<<endl;

  ObjSkipList::Iterator it(_objs);
  while(it.hasNext()) {
    it.next()->print(out);
  }

  out<<"-----------------------------"<<endl;
}


}

#endif
