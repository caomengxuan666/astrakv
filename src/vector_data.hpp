#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <hnswlib/hnswlib.h>

namespace astrakv {

enum class DistanceMetric : uint8_t {
  kL2 = 0,
  kCosine = 1,
  kInnerProduct = 2,
};

struct VectorEntry {
  std::string id;
  std::vector<float> vec;

  VectorEntry() = default;
  VectorEntry(std::string id_, std::vector<float> v)
      : id(std::move(id_)), vec(std::move(v)) {}
};

class VectorData {
public:
  VectorData(size_t dim, DistanceMetric metric = DistanceMetric::kL2,
             size_t M = 16, size_t ef_construction = 200);
  ~VectorData();

  bool Add(const std::string &id, std::span<const float> vec);
  bool Remove(const std::string &id);
  std::vector<float> Get(const std::string &id) const;
  bool Contains(const std::string &id) const;

  std::vector<std::pair<std::string, float>> Search(
      std::span<const float> query, size_t k) const;

  size_t Size() const;
  size_t Dimension() const { return dim_; }
  size_t MemoryUsage() const;

  std::vector<std::pair<std::string, std::vector<float>>> AllEntries() const;

private:
  size_t dim_;
  DistanceMetric metric_;
  std::unique_ptr<hnswlib::HierarchicalNSW<float>> index_;
  std::vector<float> id_to_vec_storage_;
  std::vector<std::string> ids_;
  size_t next_label_ = 0;
};

}  // namespace astrakv
