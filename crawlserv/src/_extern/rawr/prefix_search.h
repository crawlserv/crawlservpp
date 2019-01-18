#ifndef PREFIX_SEARCH_H_5CFCF783
#define PREFIX_SEARCH_H_5CFCF783

#include <map>
#include <string>

class prefix_search {
  public:
    void add(std::string prefix);
    int match(std::string in) const;
    
  private:
    struct node {
      std::map<int, struct node> children;
      bool match;
      
      node() : match(false) {}
    };
    
    node top;
};

#endif /* end of include guard: PREFIX_SEARCH_H_5CFCF783 */
