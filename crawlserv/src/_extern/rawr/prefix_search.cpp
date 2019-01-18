#include "prefix_search.h"

void prefix_search::add(std::string prefix)
{
  node* cur = &top;
  for (int c : prefix)
  {
    cur = &cur->children[c];
  }
  
  cur->match = true;
}

int prefix_search::match(std::string in) const
{
  int ret = 0;
  const node* cur = &top;
  for (int c : in)
  {
    if (cur->children.count(c) == 0)
    {
      return 0;
    }
    
    cur = &cur->children.at(c);
    ret++;
    
    if (cur->match)
    {
      return ret;
    }
  }
  
  return 0;
}
