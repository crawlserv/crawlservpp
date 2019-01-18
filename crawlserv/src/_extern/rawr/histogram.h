#ifndef HISTOGRAM_H_24094D97
#define HISTOGRAM_H_24094D97

#include <map>
#include <string>

template <class T>
class histogram {
  public:
    void add(const T& inst);
    void compile();
    const T& next() const;
    void print() const;
    
  private:
    std::map<T, int> freqtable;
    std::map<int, T> distribution;
};

#endif /* end of include guard: HISTOGRAM_H_24094D97 */
