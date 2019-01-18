#include "histogram.h"
#include "kgramstats.h"
#include <cstdlib>
#include <iostream>

template <class T>
void histogram<T>::add(const T& inst)
{
  freqtable[inst]++;
}

template <class T>
void histogram<T>::compile()
{
  distribution.clear();
  
  int max = 0;
  for (auto& it : freqtable)
  {
    max += it.second;
    distribution.emplace(max, it.first);
  }
  
  freqtable.clear();
}

template <class T>
const T& histogram<T>::next() const
{
  int max = distribution.rbegin()->first;
  int r = rand() % max;
  
  return distribution.upper_bound(r)->second;
}

template <class T>
void histogram<T>::print() const
{
  for (auto& freqpair : freqtable)
  {
    std::cout << freqpair.first << ": " << freqpair.second << std::endl;
  }
}

template class histogram <std::string>;
template class histogram <rawr::terminator>;
