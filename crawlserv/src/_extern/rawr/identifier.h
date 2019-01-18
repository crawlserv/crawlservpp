#ifndef IDENTIFIER_H_D7EE5679
#define IDENTIFIER_H_D7EE5679

#include <map>
#include <vector>

template <typename T>
class identifier {
public:

  using value_type = T;

private:

  using vector_type = std::vector<value_type>;

public:

  using key_type = typename vector_type::size_type;

  key_type add(const value_type& val)
  {
    auto it = ids_.find(val);

    if (it == std::end(ids_))
    {
      key_type ret = ids_.size();
      ids_[val] = ret;

      uniq_.push_back(val);

      return ret;
    } else {
      return it->second;
    }
  }

  void compile()
  {
    ids_.clear();
  }

  inline const value_type& get(key_type i) const
  {
    return uniq_.at(i);
  }

  inline key_type size() const
  {
    return uniq_.size();
  }

private:

  std::map<value_type, key_type> ids_;
  vector_type uniq_;
};

#endif /* end of include guard: IDENTIFIER_H_D7EE5679 */
