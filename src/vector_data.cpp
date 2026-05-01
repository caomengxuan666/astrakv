#include "vector_data.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace astrakv {

static std::unique_ptr<hnswlib::SpaceInterface<float>> CreateSpace(
    DistanceMetric metric, size_t dim) {
  switch (metric) {
    case DistanceMetric::kL2:
      return std::make_unique<hnswlib::L2Space>(dim);
    case DistanceMetric::kInnerProduct:
      return std::make_unique<hnswlib::InnerProductSpace>(dim);
    case DistanceMetric::kCosine:
      return std::make_unique<hnswlib::InnerProductSpace>(dim);
  }
  return nullptr;
}

VectorData::VectorData(size_t dim, DistanceMetric metric, size_t M,
                       size_t ef_construction)
    : dim_(dim), metric_(metric), next_label_(0) {
  auto space = CreateSpace(metric, dim);
  if (!space) {
    throw std::runtime_error("VectorData: failed to create distance space");
  }
  index_ = std::make_unique<hnswlib::HierarchicalNSW<float>>(
      space.get(), 1000000, M, ef_construction, 100, true);
  // space is released after HierarchicalNSW copies what it needs (v0.8.0)
}

VectorData::~VectorData() = default;

bool VectorData::Add(const std::string &id, std::span<const float> vec) {
  if (vec.size() != dim_) return false;

  hnswlib::labeltype label = static_cast<hnswlib::labeltype>(next_label_);
  try {
    index_->addPoint(const_cast<float *>(vec.data()), label, true);
  } catch (const std::exception &) {
    return false;
  }

  size_t offset = id_to_vec_storage_.size();
  id_to_vec_storage_.resize(offset + dim_);
  std::memcpy(id_to_vec_storage_.data() + offset, vec.data(),
              dim_ * sizeof(float));

  ids_.push_back(id);
  next_label_++;
  return true;
}

bool VectorData::Remove(const std::string &id) {
  auto it = std::find(ids_.begin(), ids_.end(), id);
  if (it == ids_.end()) return false;

  size_t idx = static_cast<size_t>(std::distance(ids_.begin(), it));
  hnswlib::labeltype label = static_cast<hnswlib::labeltype>(idx);
  try {
    index_->markDelete(label);
  } catch (const std::exception &) {
    return false;
  }
  return true;
}

std::vector<float> VectorData::Get(const std::string &id) const {
  auto it = std::find(ids_.begin(), ids_.end(), id);
  if (it == ids_.end()) return {};

  size_t idx = static_cast<size_t>(std::distance(ids_.begin(), it));
  size_t offset = idx * dim_;
  if (offset + dim_ > id_to_vec_storage_.size()) return {};

  return std::vector<float>(id_to_vec_storage_.begin() +
                                static_cast<ptrdiff_t>(offset),
                            id_to_vec_storage_.begin() +
                                static_cast<ptrdiff_t>(offset + dim_));
}

bool VectorData::Contains(const std::string &id) const {
  return std::find(ids_.begin(), ids_.end(), id) != ids_.end();
}

std::vector<std::pair<std::string, float>> VectorData::Search(
    std::span<const float> query, size_t k) const {
  if (query.size() != dim_ || ids_.empty()) return {};

  size_t search_k = std::min(k, ids_.size());
  if (search_k == 0) return {};

  try {
    auto result = index_->searchKnn(const_cast<float *>(query.data()),
                                    search_k);

    std::vector<std::pair<std::string, float>> results;
    results.reserve(result.size());

    while (!result.empty()) {
      auto &top = result.top();
      size_t label = static_cast<size_t>(top.second);
      if (label < ids_.size()) {
        results.emplace_back(ids_[label], top.first);
      }
      result.pop();
    }

    std::reverse(results.begin(), results.end());
    return results;
  } catch (const std::exception &) {
    return {};
  }
}

size_t VectorData::Size() const { return ids_.size(); }

size_t VectorData::MemoryUsage() const {
  return id_to_vec_storage_.size() * sizeof(float) +
         ids_.size() * sizeof(std::string) +
         ids_.size() * 128;
}

std::vector<std::pair<std::string, std::vector<float>>>
VectorData::AllEntries() const {
  std::vector<std::pair<std::string, std::vector<float>>> entries;
  entries.reserve(ids_.size());
  for (size_t i = 0; i < ids_.size(); ++i) {
    size_t offset = i * dim_;
    std::vector<float> v(id_to_vec_storage_.begin() + static_cast<ptrdiff_t>(offset),
                         id_to_vec_storage_.begin() +
                             static_cast<ptrdiff_t>(offset + dim_));
    entries.emplace_back(ids_[i], std::move(v));
  }
  return entries;
}

}  // namespace astrakv
