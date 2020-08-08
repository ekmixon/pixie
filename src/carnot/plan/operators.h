#pragma once

#include <stddef.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/wrappers.pb.h>

#include "src/carnot/plan/plan_node.h"
#include "src/carnot/plan/plan_state.h"
#include "src/carnot/plan/scalar_expression.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/column_wrapper.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace plan {

// Forward declare to break deps.
class PlanState;

/**
 * Operator is the pure virtual base class for logical plan nodes that perform
 * operations on data/metadata.
 */
class Operator : public PlanNode {
 public:
  ~Operator() override = default;

  // Create a new operator using the Operator proto.
  static std::unique_ptr<Operator> FromProto(const planpb::Operator& pb, int64_t id);

  // Returns the ID of the operator.
  int64_t id() const { return id_; }

  // Returns the type of the operator.
  planpb::OperatorType op_type() const { return op_type_; }

  bool is_initialized() const { return is_initialized_; }

  // Generate a string that will help debug operators.
  std::string DebugString() const override = 0;

  // Prints out the debug to INFO log.
  void Debug() { LOG(INFO) << DebugString(); }

  virtual StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const = 0;

 protected:
  Operator(int64_t id, planpb::OperatorType op_type) : id_(id), op_type_(op_type) {}

  int64_t id_;
  planpb::OperatorType op_type_;
  // Tracks if the operator has been fully initialized.
  bool is_initialized_ = false;
};

class MemorySourceOperator : public Operator {
 public:
  explicit MemorySourceOperator(int64_t id) : Operator(id, planpb::MEMORY_SOURCE_OPERATOR) {}
  ~MemorySourceOperator() override = default;
  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::MemorySourceOperator& pb);
  std::string DebugString() const override;
  std::string TableName() const { return pb_.name(); }
  bool HasStartTime() const { return pb_.has_start_time(); }
  bool HasStopTime() const { return pb_.has_stop_time(); }
  int64_t start_time() const { return pb_.start_time().value(); }
  int64_t stop_time() const { return pb_.stop_time().value(); }
  std::vector<int64_t> Columns() const { return column_idxs_; }
  const types::TabletID& Tablet() const { return pb_.tablet(); }

 private:
  planpb::MemorySourceOperator pb_;

  std::vector<int64_t> column_idxs_;
};

class MapOperator : public Operator {
 public:
  explicit MapOperator(int64_t id) : Operator(id, planpb::MAP_OPERATOR) {}
  ~MapOperator() override = default;

  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::MapOperator& pb);
  std::string DebugString() const override;

  const std::vector<std::shared_ptr<const ScalarExpression>>& expressions() const {
    return expressions_;
  }

 private:
  std::vector<std::shared_ptr<const ScalarExpression>> expressions_;
  std::vector<std::string> column_names_;

  planpb::MapOperator pb_;
};

class AggregateOperator : public Operator {
 public:
  explicit AggregateOperator(int64_t id) : Operator(id, planpb::AGGREGATE_OPERATOR) {}
  ~AggregateOperator() override = default;

  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::AggregateOperator& pb);
  std::string DebugString() const override;

  struct GroupInfo {
    std::string name;
    uint64_t idx;
  };

  const std::vector<GroupInfo>& groups() const { return groups_; }
  const std::vector<std::shared_ptr<AggregateExpression>>& values() const { return values_; }
  bool windowed() const { return pb_.windowed(); }

 private:
  std::vector<std::shared_ptr<AggregateExpression>> values_;
  std::vector<GroupInfo> groups_;
  planpb::AggregateOperator pb_;
};

class MemorySinkOperator : public Operator {
 public:
  explicit MemorySinkOperator(int64_t id) : Operator(id, planpb::MEMORY_SINK_OPERATOR) {}
  ~MemorySinkOperator() override = default;

  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::MemorySinkOperator& pb);
  std::string TableName() const { return pb_.name(); }
  std::string ColumnName(int64_t i) const { return pb_.column_names(i); }
  std::string DebugString() const override;

 private:
  planpb::MemorySinkOperator pb_;
};

class GRPCSourceOperator : public Operator {
 public:
  explicit GRPCSourceOperator(int64_t id) : Operator(id, planpb::GRPC_SOURCE_OPERATOR) {}
  ~GRPCSourceOperator() override = default;

  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::GRPCSourceOperator& pb);
  std::string DebugString() const override;

 private:
  planpb::GRPCSourceOperator pb_;
};

class GRPCSinkOperator : public Operator {
 public:
  explicit GRPCSinkOperator(int64_t id) : Operator(id, planpb::GRPC_SINK_OPERATOR) {}
  ~GRPCSinkOperator() override = default;

  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::GRPCSinkOperator& pb);
  std::string DebugString() const override;

  std::string address() const { return pb_.address(); }

  bool has_grpc_source_id() const {
    LOG(ERROR) << absl::Substitute("DebugString: $0", pb_.DebugString());
    return pb_.destination_case() == planpb::GRPCSinkOperator::kGrpcSourceId;
  }
  int64_t grpc_source_id() const { return pb_.grpc_source_id(); }

  bool has_table_name() const {
    return pb_.destination_case() == planpb::GRPCSinkOperator::kTableName;
  }
  std::string table_name() const { return pb_.table_name(); }

 private:
  planpb::GRPCSinkOperator pb_;
};

class FilterOperator : public Operator {
 public:
  explicit FilterOperator(int64_t id) : Operator(id, planpb::FILTER_OPERATOR) {}
  ~FilterOperator() override = default;

  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::FilterOperator& pb);
  std::string DebugString() const override;
  std::vector<int64_t> selected_cols() { return selected_cols_; }

  const std::shared_ptr<const ScalarExpression>& expression() const { return expression_; }

 private:
  std::shared_ptr<const ScalarExpression> expression_;
  std::vector<int64_t> selected_cols_;
  planpb::FilterOperator pb_;
};

class LimitOperator : public Operator {
 public:
  explicit LimitOperator(int64_t id) : Operator(id, planpb::LIMIT_OPERATOR) {}
  ~LimitOperator() override = default;

  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::LimitOperator& pb);
  std::string DebugString() const override;
  std::vector<int64_t> selected_cols() { return selected_cols_; }

  int64_t record_limit() const { return record_limit_; }

 private:
  int64_t record_limit_ = 0;
  std::vector<int64_t> selected_cols_;
  planpb::LimitOperator pb_;
};

class UnionOperator : public Operator {
 public:
  explicit UnionOperator(int64_t id) : Operator(id, planpb::UNION_OPERATOR) {}
  ~UnionOperator() override = default;

  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::UnionOperator& pb);
  std::string DebugString() const override;

  const std::vector<std::string>& column_names() const { return column_names_; }
  bool order_by_time() const;
  int64_t time_column_index(int64_t parent_index) const;
  const std::vector<std::vector<int64_t>>& column_mappings() const { return column_mappings_; }
  const std::vector<int64_t>& column_mapping(int64_t parent_index) const {
    return column_mappings_.at(parent_index);
  }
  size_t rows_per_batch() const { return pb_.rows_per_batch(); }

 private:
  std::vector<std::string> column_names_;
  std::vector<std::vector<int64_t>> column_mappings_;

  planpb::UnionOperator pb_;
};

class JoinOperator : public Operator {
 public:
  explicit JoinOperator(int64_t id) : Operator(id, planpb::JOIN_OPERATOR) {}
  ~JoinOperator() override = default;

  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::JoinOperator& pb);
  std::string DebugString() const override;

  // Static debug functions on the helper classes.
  // These can also be for printing debug information in both the operator and exec node.
  static std::string DebugString(planpb::JoinOperator::JoinType type);
  static std::string DebugString(
      const std::vector<planpb::JoinOperator::EqualityCondition>& conditions);

  const std::vector<std::string>& column_names() const { return column_names_; }
  planpb::JoinOperator::JoinType type() const { return pb_.type(); }
  std::vector<planpb::JoinOperator::EqualityCondition> equality_conditions() const {
    return equality_conditions_;
  }
  std::vector<planpb::JoinOperator::ParentColumn> output_columns() const { return output_columns_; }
  size_t rows_per_batch() const { return pb_.rows_per_batch(); }

  bool order_by_time() const;
  planpb::JoinOperator::ParentColumn time_column() const;

 private:
  std::vector<std::string> column_names_;
  std::vector<planpb::JoinOperator::EqualityCondition> equality_conditions_;
  std::vector<planpb::JoinOperator::ParentColumn> output_columns_;

  planpb::JoinOperator pb_;
};

class UDTFSourceOperator : public Operator {
 public:
  explicit UDTFSourceOperator(int64_t id) : Operator(id, planpb::UDTF_SOURCE_OPERATOR) {}
  ~UDTFSourceOperator() override = default;
  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::UDTFSourceOperator& pb);
  std::string DebugString() const override;
  const std::string& name() const { return pb_.name(); }
  const std::vector<ScalarValue>& init_arguments() const;

 private:
  planpb::UDTFSourceOperator pb_;
  std::vector<ScalarValue> init_arguments_;
};

}  // namespace plan
}  // namespace carnot
}  // namespace pl
