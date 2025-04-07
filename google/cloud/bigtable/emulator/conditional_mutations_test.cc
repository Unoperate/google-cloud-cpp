#include "google/cloud/bigtable/emulator/table.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/bigtable/v2/bigtable.pb.h>
#include <google/bigtable/v2/data.pb.h>
#include <gtest/gtest.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

struct SetCellParams {
  std::string column_family_name;
  std::string column_qualifier;
  int64_t timestamp_micros;
  std::string data;
};

StatusOr<std::shared_ptr<Table>> create_table(
    std::string const& table_name, std::vector<std::string>& column_families) {
  ::google::bigtable::admin::v2::Table schema;
  schema.set_name(table_name);
  for (auto& column_family_name : column_families) {
    (*schema.mutable_column_families())[column_family_name] =
        ::google::bigtable::admin::v2::ColumnFamily();
  }

  return Table::Create(schema);
}

TEST(ConditionalMutations, RejectInvalidRequest) {
  auto const* const table_name = "projects/test/instances/test/tables/test";
  auto const* const column_family_name = "test_column_family";
  auto const* const row_key = "0";
  auto const* const column_qualifer = "column_1";
  auto timestamp_micros = 1000;
  auto const* const true_mutation_value = "set by a true mutation";
  auto const* const false_mutation_value = "set by a false mutation";

  std::vector<std::string> column_families = {column_family_name};
  auto maybe_table = create_table(table_name, column_families);

  ASSERT_STATUS_OK(maybe_table);
  auto table = maybe_table.value();

  ::google::bigtable::v2::Mutation true_mutation;
  auto* set_cell_mutation = true_mutation.mutable_set_cell();
  set_cell_mutation->set_family_name(column_family_name);
  set_cell_mutation->set_column_qualifier(column_qualifer);
  set_cell_mutation->set_timestamp_micros(timestamp_micros);
  set_cell_mutation->set_value(true_mutation_value);

  std::vector<google::bigtable::v2::Mutation> true_mutations = {true_mutation};

  ::google::bigtable::v2::Mutation false_mutation;
  set_cell_mutation = false_mutation.mutable_set_cell();
  set_cell_mutation->set_family_name(column_family_name);
  set_cell_mutation->set_column_qualifier(column_qualifer);
  set_cell_mutation->set_timestamp_micros(timestamp_micros);
  set_cell_mutation->set_value(false_mutation_value);

  // Will be configured so that row_key is not set.
  std::vector<google::bigtable::v2::Mutation> false_mutations = {
      false_mutation};

  google::bigtable::v2::CheckAndMutateRowRequest cond_mutation_no_row_key;

  cond_mutation_no_row_key.set_table_name(table_name);
  cond_mutation_no_row_key.mutable_true_mutations()->Assign(
      true_mutations.begin(), true_mutations.end());
  cond_mutation_no_row_key.mutable_false_mutations()->Assign(
      false_mutations.begin(), false_mutations.end());

  auto status_or = table->CheckAndMutateRow(cond_mutation_no_row_key);
  ASSERT_EQ(false, status_or.ok());

  // Will be configured so that both true_mutations and
  // false_mutations are empty.
  google::bigtable::v2::CheckAndMutateRowRequest cond_mutation_no_mutations;
  cond_mutation_no_mutations.set_row_key(row_key);
  cond_mutation_no_row_key.set_table_name(table_name);
  ASSERT_EQ(false, table->CheckAndMutateRow(cond_mutation_no_mutations).ok());
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
