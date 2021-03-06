#pragma once

#include <boost/algorithm/string.hpp>
#include <sstream>
#include <vector>

namespace marian {
namespace data {

class WordAlignment {
private:
  typedef std::pair<size_t, size_t> Point;
  std::vector<Point> data_;

public:
  WordAlignment();

  /**
   * @brief Constructs word alignments from a vector of pairs of two integers.
   *
   * @param align Vector of pairs of two unsigned integers
   */
  WordAlignment(const std::vector<std::pair<size_t, size_t>>& align);

  /**
   * @brief Constructs word alignments from textual representation.
   *
   * @param line String in the form of "0-0 1-1 1-2", etc.
   */
  WordAlignment(const std::string& line);

  auto begin() const -> decltype(data_.begin()) { return data_.begin(); }
  auto end() const -> decltype(data_.end()) { return data_.end(); }

  void push_back(size_t s, size_t t) { data_.push_back(std::make_pair(s, t)); }

  size_t size() const { return data_.size(); }

  /**
   * @brief Sorts alignments in place by source indices in ascending order.
   */
  void sort();

  /**
   * @brief Returns textual representation.
   */
  std::string toString() const;
};

typedef std::vector<std::vector<float>> SoftAlignment;

WordAlignment ConvertSoftAlignToHardAlign(SoftAlignment alignSoft,
                                          float threshold = 1.f,
                                          bool reversed = true,
                                          bool skipEOS = false);

}  // namespace data
}  // namespace marian
