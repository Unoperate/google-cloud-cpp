// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/emulator/column_family.h"
#include <absl/types/optional.h>
#include <google/bigtable/v2/types.pb.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <array>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

uint64_t BigEndianToUint64(std::string const& s) {
  if (s.length() != 8) {
    std::abort();  // We expect to be called with a  8 byte string in big-endian
                   // encoding only.
  }
  auto const* u_bytes = reinterpret_cast<uint8_t const*>(s.c_str());

  return static_cast<uint64_t>(u_bytes[7]) |
         static_cast<uint64_t>(u_bytes[6]) << 8 |
         static_cast<uint64_t>(u_bytes[5]) << 16 |
         static_cast<uint64_t>(u_bytes[4]) << 24 |
         static_cast<uint64_t>(u_bytes[3]) << 32 |
         static_cast<uint64_t>(u_bytes[2]) << 40 |
         static_cast<uint64_t>(u_bytes[1]) << 48 |
         static_cast<uint64_t>(u_bytes[0]) << 56;
}

std::string Uint64ToBigEndian(uint64_t i) {
  std::array<uint8_t, 8> u_bytes;

  u_bytes[0] = static_cast<uint8_t>(i >> 56);
  u_bytes[1] = static_cast<uint8_t>(i >> 48);
  u_bytes[2] = static_cast<uint8_t>(i >> 40);
  u_bytes[3] = static_cast<uint8_t>(i >> 32);
  u_bytes[4] = static_cast<uint8_t>(i >> 24);
  u_bytes[5] = static_cast<uint8_t>(i >> 16);
  u_bytes[6] = static_cast<uint8_t>(i >> 8);
  u_bytes[7] = static_cast<uint8_t>(i);

  std::string ret(std::begin(u_bytes), std::end(u_bytes));

  return ret;
}

absl::optional<std::string> ColumnRow::SetCell(
    std::chrono::milliseconds timestamp, std::string const& value) {
  if (timestamp <= std::chrono::milliseconds::zero()) {
    timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
  }

  absl::optional<std::string> ret = absl::nullopt;
  auto cell_it = cells_.find(timestamp);
  if (!(cell_it == cells_.end())) {
    ret = std::move(cell_it->second);
  }

  cells_[timestamp] = value;

  return ret;
}

std::vector<Cell> ColumnRow::DeleteTimeRange(
    ::google::bigtable::v2::TimestampRange const& time_range) {
  std::vector<Cell> deleted_cells;
  for (auto cell_it = cells_.lower_bound(
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds(time_range.start_timestamp_micros())));
       cell_it != cells_.end() &&
       (time_range.end_timestamp_micros() == 0 ||
        cell_it->first < std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::microseconds(
                                 time_range.end_timestamp_micros())));) {
    Cell cell = {std::move(cell_it->first), std::move(cell_it->second)};
    deleted_cells.emplace_back(std::move(cell));
    cells_.erase(cell_it++);
  }
  return deleted_cells;
}

absl::optional<Cell> ColumnRow::DeleteTimeStamp(
    std::chrono::milliseconds timestamp) {
  absl::optional<Cell> ret = absl::nullopt;

  auto cell_it = cells_.find(timestamp);
  if (cell_it != cells_.end()) {
    Cell cell = {std::move(cell_it->first), std::move(cell_it->second)};
    ret.emplace(std::move(cell));
    cells_.erase(cell_it);
  }

  return ret;
}

absl::optional<std::string> ColumnFamilyRow::SetCell(
    std::string const& column_qualifier, std::chrono::milliseconds timestamp,
    std::string const& value) {
  return columns_[column_qualifier].SetCell(timestamp, value);
}

std::vector<Cell> ColumnFamilyRow::DeleteColumn(
    std::string const& column_qualifier,
    ::google::bigtable::v2::TimestampRange const& time_range) {
  auto column_it = columns_.find(column_qualifier);
  if (column_it == columns_.end()) {
    return {};
  }
  auto res = column_it->second.DeleteTimeRange(time_range);
  if (!column_it->second.HasCells()) {
    columns_.erase(column_it);
  }
  return res;
}

absl::optional<Cell> ColumnFamilyRow::DeleteTimeStamp(
    std::string const& column_qulifier, std::chrono::milliseconds timestamp) {
  auto column_it = columns_.find(column_qulifier);
  if (column_it == columns_.end()) {
    return absl::nullopt;
  }

  auto ret = column_it->second.DeleteTimeStamp(timestamp);
  if (!column_it->second.HasCells()) {
    columns_.erase(column_it);
  }

  return ret;
}

absl::optional<std::string> ColumnFamily::SetCell(
    std::string const& row_key, std::string const& column_qualifier,
    std::chrono::milliseconds timestamp, std::string const& value) {
  // To support complex types (e.g. aggregations), check if a cell
  // with the timestamp already exists. If it does, derive a new value
  // to set by calling UpdateCell_ with the existing and new values.
  std::string update_value;
  auto column_family_row_it = find(row_key);
  if (column_family_row_it != end()) {
    auto column_it = column_family_row_it->second.find(column_qualifier);
    if (column_it != column_family_row_it->second.end()) {
      auto column_row_it = column_it->second.find(timestamp);
      if (column_row_it != column_it->second.end()) {
        // We are updating an existing cell
        update_value = UpdateCell_(column_row_it->second, value);
        return rows_[row_key].SetCell(column_qualifier, timestamp,
                                      update_value);
      }
    }
  }

  // FIXME: Also to support aggregation of complex types, we
  // definitely also need an InitializeCell_ function since some
  // aggregation types may require special treatment of an initial
  // value. However for now the aggregations that we support, Sum, Min
  // and Max do not requre this.

  return rows_[row_key].SetCell(column_qualifier, timestamp, value);
}

std::map<std::string, std::vector<Cell>> ColumnFamily::DeleteRow(
    std::string const& row_key) {
  std::map<std::string, std::vector<Cell>> res;

  auto row_it = rows_.find(row_key);
  if (row_it == rows_.end()) {
    return {};
  }

  for (auto& column : row_it->second.columns_) {
    // Not setting start and end timestamps will select all cells for deletion
    ::google::bigtable::v2::TimestampRange time_range;
    auto deleted_cells = column.second.DeleteTimeRange(time_range);
    if (!deleted_cells.empty()) {
      res[std::move(column.first)] = std::move(deleted_cells);
    }
  }

  return res;
}

std::vector<Cell> ColumnFamily::DeleteColumn(
    std::string const& row_key, std::string const& column_qualifier,
    ::google::bigtable::v2::TimestampRange const& time_range) {
  auto row_it = rows_.find(row_key);

  return DeleteColumn(row_it, column_qualifier, time_range);
}

std::vector<Cell> ColumnFamily::DeleteColumn(
    std::map<std::string, ColumnFamilyRow>::iterator row_it,
    std::string const& column_qualifier,
    ::google::bigtable::v2::TimestampRange const& time_range) {
  if (row_it != rows_.end()) {
    auto erased_cells =
        row_it->second.DeleteColumn(column_qualifier, time_range);
    if (!row_it->second.HasColumns()) {
      rows_.erase(row_it);
    }
    return erased_cells;
  }
  return {};
}

absl::optional<Cell> ColumnFamily::DeleteTimeStamp(
    std::string const& row_key, std::string const& column_qulifier,
    std::chrono::milliseconds timestamp) {
  auto row_it = rows_.find(row_key);
  if (row_it == rows_.end()) {
    return absl::nullopt;
  }

  auto ret = row_it->second.DeleteTimeStamp(column_qulifier, timestamp);
  if (!row_it->second.HasColumns()) {
    rows_.erase(row_it);
  }

  return ret;
}

class FilteredColumnFamilyStream::FilterApply {
 public:
  explicit FilterApply(FilteredColumnFamilyStream& parent) : parent_(parent) {}

  bool operator()(ColumnRange const& column_range) {
    if (column_range.column_family == parent_.column_family_name_) {
      parent_.column_ranges_.Intersect(column_range.range);
    }
    return true;
  }

  bool operator()(TimestampRange const& timestamp_range) {
    parent_.timestamp_ranges_.Intersect(timestamp_range.range);
    return true;
  }

  bool operator()(RowKeyRegex const& row_key_regex) {
    parent_.row_regexes_.emplace_back(row_key_regex.regex);
    return true;
  }

  bool operator()(FamilyNameRegex const&) { return false; }

  bool operator()(ColumnRegex const& column_regex) {
    parent_.column_regexes_.emplace_back(column_regex.regex);
    return true;
  }

 private:
  FilteredColumnFamilyStream& parent_;
};

FilteredColumnFamilyStream::FilteredColumnFamilyStream(
    ColumnFamily const& column_family, std::string column_family_name,
    std::shared_ptr<StringRangeSet const> row_set)
    : column_family_name_(std::move(column_family_name)),
      row_ranges_(std::move(row_set)),
      column_ranges_(StringRangeSet::All()),
      timestamp_ranges_(TimestampRangeSet::All()),
      rows_(RangeFilteredMapView<ColumnFamily, StringRangeSet>(column_family,
                                                               *row_ranges_),
            std::cref(row_regexes_)) {}

bool FilteredColumnFamilyStream::ApplyFilter(
    InternalFilter const& internal_filter) {
  assert(!initialized_);
  return absl::visit(FilterApply(*this), internal_filter);
}

bool FilteredColumnFamilyStream::HasValue() const {
  InitializeIfNeeded();
  return *row_it_ != rows_.end();
}
CellView const& FilteredColumnFamilyStream::Value() const {
  InitializeIfNeeded();
  if (!cur_value_) {
    cur_value_ = CellView((*row_it_)->first, column_family_name_,
                          column_it_.value()->first, cell_it_.value()->first,
                          cell_it_.value()->second);
  }
  return cur_value_.value();
}

bool FilteredColumnFamilyStream::Next(NextMode mode) {
  InitializeIfNeeded();
  cur_value_.reset();
  assert(*row_it_ != rows_.end());
  assert(column_it_.value() != columns_.value().end());
  assert(cell_it_.value() != cells_.value().end());

  if (mode == NextMode::kCell) {
    ++(cell_it_.value());
    if (cell_it_.value() != cells_.value().end()) {
      return true;
    }
  }
  if (mode == NextMode::kCell || mode == NextMode::kColumn) {
    ++(column_it_.value());
    if (PointToFirstCellAfterColumnChange()) {
      return true;
    }
  }
  ++(*row_it_);
  PointToFirstCellAfterRowChange();
  return true;
}

void FilteredColumnFamilyStream::InitializeIfNeeded() const {
  if (!initialized_) {
    row_it_ = rows_.begin();
    PointToFirstCellAfterRowChange();
    initialized_ = true;
  }
}

bool FilteredColumnFamilyStream::PointToFirstCellAfterColumnChange() const {
  for (; column_it_.value() != columns_.value().end(); ++(column_it_.value())) {
    cells_ = RangeFilteredMapView<ColumnRow, TimestampRangeSet>(
        column_it_.value()->second, timestamp_ranges_);
    cell_it_ = cells_.value().begin();
    if (cell_it_.value() != cells_.value().end()) {
      return true;
    }
  }
  return false;
}

bool FilteredColumnFamilyStream::PointToFirstCellAfterRowChange() const {
  for (; (*row_it_) != rows_.end(); ++(*row_it_)) {
    columns_ = RegexFiteredMapView<
        RangeFilteredMapView<ColumnFamilyRow, StringRangeSet>>(
        RangeFilteredMapView<ColumnFamilyRow, StringRangeSet>(
            (*row_it_)->second, column_ranges_),
        column_regexes_);
    column_it_ = columns_.value().begin();
    if (PointToFirstCellAfterColumnChange()) {
      return true;
    }
  }
  return false;
}

ColumnFamily::ColumnFamily(
    absl::optional<google::bigtable::admin::v2::Type> value_type) {
  value_type_ = std::move(value_type);

  if (!value_type_.has_value()) {
    return;
  }

  // FIXME: We currently only support big-endian uint64
  // encoding. Check that the intent matches that as well (big-endian
  // encoding in so-called ordered mode that supports only
  // non-negative integers). However, the mutation code such as the one
  // for AddCell will check this as well and reject mutations with
  // encoding we don't support or negative numbers.
  if (value_type_.value().has_aggregate_type()) {
    auto aggregate_type = value_type_.value().aggregate_type();
    switch (aggregate_type.aggregator_case()) {
      case google::bigtable::admin::v2::Type::Aggregate::kSum:
        UpdateCell_ = Sum_UpdateCell_BE_Uint64;
        break;
      case google::bigtable::admin::v2::Type::Aggregate::kMin:
        UpdateCell_ = Min_UpdateCell_BE_Uint64;
        break;
      case google::bigtable::admin::v2::Type::Aggregate::kMax:
        UpdateCell_ = Max_UpdateCell_BE_Uint64;
        break;
      default:
        break;
    }
  }
};

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
