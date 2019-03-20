#include "src/carnot/compiler/ir_nodes.h"

namespace pl {
namespace carnot {
namespace compiler {

Status IRUtils::CreateIRNodeError(const std::string& err_msg, const IRNode& node) {
  return error::InvalidArgument("Line $0 Col $1 : $2", node.line(), node.col(), err_msg);
}

Status IR::AddEdge(int64_t from_node, int64_t to_node) {
  dag_.AddEdge(from_node, to_node);
  return Status::OK();
}

Status IR::AddEdge(IRNode* from_node, IRNode* to_node) {
  return AddEdge(from_node->id(), to_node->id());
}

void IR::DeleteEdge(int64_t from_node, int64_t to_node) { dag_.DeleteEdge(from_node, to_node); }

void IR::DeleteNode(int64_t node) { dag_.DeleteNode(node); }

std::string IR::DebugString() {
  std::string debug_string = absl::StrFormat("%s\n", dag().DebugString());
  for (auto const& a : id_node_map_) {
    debug_string += absl::StrFormat("%s\n", a.second->DebugString(0));
  }
  return debug_string;
}

void IRNode::SetLineCol(int64_t line, int64_t col) {
  line_ = line;
  col_ = col;
  line_col_set_ = true;
}
void IRNode::SetLineCol(const pypa::AstPtr& ast_node) {
  ast_node_ = ast_node;
  SetLineCol(ast_node->line, ast_node->column);
}

Status OperatorIR::SetParent(IRNode* node) {
  if (!node->IsOp()) {
    return error::InvalidArgument("Expected Op, got $0 instead", node->type_string());
  }
  parent_ = static_cast<OperatorIR*>(node);
  return Status::OK();
}

bool MemorySourceIR::HasLogicalRepr() const { return true; }

Status MemorySourceIR::Init(IRNode* table_node, IRNode* select, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  table_node_ = table_node;
  select_ = select;
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, select_));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, table_node_));

  return Status::OK();
}

Status MemorySourceIR::ToProto(carnotpb::Operator* op) const {
  auto pb = new carnotpb::MemorySourceOperator();
  DCHECK(table_node_->type() == IRNodeType::StringType);
  pb->set_name(static_cast<StringIR*>(table_node_)->str());

  if (!columns_set()) {
    return error::InvalidArgument("MemorySource columns are not set.");
  }

  for (const auto& col : columns_) {
    pb->add_column_idxs(col->col_idx());
    pb->add_column_names(col->col_name());
    pb->add_column_types(col->type());
  }

  if (IsTimeSet()) {
    auto start_time = new ::google::protobuf::Int64Value();
    start_time->set_value(time_start_ns_);
    pb->set_allocated_start_time(start_time);
    auto stop_time = new ::google::protobuf::Int64Value();
    stop_time->set_value(time_stop_ns_);
    pb->set_allocated_stop_time(stop_time);
  }

  op->set_op_type(carnotpb::MEMORY_SOURCE_OPERATOR);
  op->set_allocated_mem_source_op(pb);
  return Status::OK();
}

std::string DebugStringFmt(int64_t depth, std::string name,
                           std::map<std::string, std::string> property_value_map) {
  std::vector<std::string> property_strings;
  std::map<std::string, std::string>::iterator it;
  std::string depth_string = std::string(depth, '\t');
  property_strings.push_back(absl::StrFormat("%s%s", depth_string, name));

  for (it = property_value_map.begin(); it != property_value_map.end(); it++) {
    std::string prop_str = absl::Substitute("$0 $1\t-$2", depth_string, it->first, it->second);
    property_strings.push_back(prop_str);
  }
  return absl::StrJoin(property_strings, "\n");
}
std::string MemorySourceIR::DebugString(int64_t depth) const {
  return DebugStringFmt(
      depth, absl::StrFormat("%d:MemorySourceIR", id()),
      {{"From", table_node_->DebugString(depth + 1)}, {"Select", select_->DebugString(depth + 1)}});
}

bool MemorySinkIR::HasLogicalRepr() const { return true; }

Status MemorySinkIR::Init(IRNode* parent_node, const std::string& name,
                          const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  PL_RETURN_IF_ERROR(SetParent(parent_node));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(parent(), this));
  name_ = name;
  name_set_ = true;

  return Status::OK();
}

std::string MemorySinkIR::DebugString(int64_t depth) const {
  return DebugStringFmt(depth, absl::StrFormat("%d:MemorySinkIR", id()),
                        {{"Parent", parent()->DebugString(depth + 1)}});
}

Status MemorySinkIR::ToProto(carnotpb::Operator* op) const {
  auto pb = new carnotpb::MemorySinkOperator();
  pb->set_name(name_);

  auto types = relation().col_types();
  auto names = relation().col_names();

  for (size_t i = 0; i < relation().NumColumns(); i++) {
    pb->add_column_types(types[i]);
    pb->add_column_names(names[i]);
  }

  op->set_op_type(carnotpb::MEMORY_SINK_OPERATOR);
  op->set_allocated_mem_sink_op(pb);
  return Status::OK();
}

Status RangeIR::Init(IRNode* parent_node, IRNode* start_repr, IRNode* stop_repr,
                     const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  PL_RETURN_IF_ERROR(SetParent(parent_node));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(parent(), this));
  return SetStartStop(start_repr, stop_repr);
}
Status RangeIR::SetStartStop(IRNode* start_repr, IRNode* stop_repr) {
  if (start_repr_) {
    graph_ptr()->DeleteEdge(id(), start_repr_->id());
  }
  if (stop_repr_) {
    graph_ptr()->DeleteEdge(id(), stop_repr_->id());
  }
  start_repr_ = start_repr;
  stop_repr_ = stop_repr;
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, start_repr_));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, stop_repr_));
  return Status::OK();
}

bool RangeIR::HasLogicalRepr() const { return false; }

std::string RangeIR::DebugString(int64_t depth) const {
  return DebugStringFmt(depth, absl::StrFormat("%d:RangeIR", id()),
                        {{"Parent", parent()->DebugString(depth + 1)},
                         {"Start", start_repr_->DebugString(depth + 1)},
                         {"Stop", stop_repr_->DebugString(depth + 1)}});
}

Status RangeIR::ToProto(carnotpb::Operator*) const {
  return error::InvalidArgument("RangeIR has no protobuf representation.");
}

Status MapIR::Init(IRNode* parent_node, IRNode* lambda_func, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  lambda_func_ = lambda_func;
  PL_RETURN_IF_ERROR(SetParent(parent_node));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, lambda_func_));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(parent(), this));
  return Status::OK();
}

bool MapIR::HasLogicalRepr() const { return true; }

std::string MapIR::DebugString(int64_t depth) const {
  return DebugStringFmt(depth, absl::StrFormat("%d:MapIR", id()),
                        {{"Parent", parent()->DebugString(depth + 1)},
                         {"Lambda", lambda_func_->DebugString(depth + 1)}});
}

Status MapIR::EvaluateExpression(carnotpb::ScalarExpression* expr, const IRNode& ir_node) const {
  switch (ir_node.type()) {
    case IRNodeType::ColumnType: {
      auto col = expr->mutable_column();
      col->set_node(parent()->id());
      col->set_index(static_cast<const ColumnIR&>(ir_node).col_idx());
      break;
    }
    case IRNodeType::FuncType: {
      auto func = expr->mutable_func();
      auto casted_ir = static_cast<const FuncIR&>(ir_node);
      func->set_name(casted_ir.func_name());
      for (const auto& arg : casted_ir.args()) {
        auto func_arg = func->add_args();
        PL_RETURN_IF_ERROR(EvaluateExpression(func_arg, *arg));
      }
      break;
    }
    case IRNodeType::IntType: {
      auto value = expr->mutable_constant();
      auto casted_ir = static_cast<const IntIR&>(ir_node);
      value->set_data_type(types::DataType::INT64);
      value->set_int64_value(casted_ir.val());
      break;
    }
    case IRNodeType::StringType: {
      auto value = expr->mutable_constant();
      auto casted_ir = static_cast<const StringIR&>(ir_node);
      value->set_data_type(types::DataType::STRING);
      value->set_string_value(casted_ir.str());
      break;
    }
    case IRNodeType::FloatType: {
      auto value = expr->mutable_constant();
      auto casted_ir = static_cast<const FloatIR&>(ir_node);
      value->set_data_type(types::DataType::FLOAT64);
      value->set_float64_value(casted_ir.val());
      break;
    }
    case IRNodeType::BoolType: {
      auto value = expr->mutable_constant();
      auto casted_ir = static_cast<const BoolIR&>(ir_node);
      value->set_data_type(types::DataType::BOOLEAN);
      value->set_bool_value(casted_ir.val());
      break;
    }
    case IRNodeType::TimeType: {
      auto value = expr->mutable_constant();
      auto casted_ir = static_cast<const TimeIR&>(ir_node);
      value->set_data_type(types::DataType::TIME64NS);
      value->set_time64_ns_value(static_cast<::google::protobuf::int64>(casted_ir.val()));
      break;
    }
    default: {
      return error::InvalidArgument("Didn't expect node of type $0 in expression evaluator.",
                                    ir_node.type_string());
    }
  }
  return Status::OK();
}

Status MapIR::ToProto(carnotpb::Operator* op) const {
  auto pb = new carnotpb::MapOperator();

  for (const auto& col_expr : col_exprs_) {
    auto expr = pb->add_expressions();
    PL_RETURN_IF_ERROR(EvaluateExpression(expr, *col_expr.node));
    pb->add_column_names(col_expr.name);
  }

  op->set_op_type(carnotpb::MAP_OPERATOR);
  op->set_allocated_map_op(pb);
  return Status::OK();
}

Status BlockingAggIR::Init(IRNode* parent_node, IRNode* by_func, IRNode* agg_func,
                           const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  if (agg_func->type() != IRNodeType::LambdaType) {
    return IRUtils::CreateIRNodeError(
        absl::StrFormat("Expected 'agg' argument of BlockingAggIR to be 'Lambda', got '%s'",
                        agg_func->type_string()),
        *agg_func);
  }
  // If by_func_ is not a null pointer, then update the graph with it. Otherwise, continue onwards.
  if (by_func->type() == IRNodeType::BoolType) {
    by_func_ = nullptr;

  } else if (by_func->type() == IRNodeType::LambdaType) {
    PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, by_func));
    by_func_ = by_func;
  } else {
    return IRUtils::CreateIRNodeError(
        absl::StrFormat("Expected 'by' argument of AggIR to be 'Lambda', got '%s'",
                        by_func->type_string()),
        *by_func);
  }

  agg_func_ = agg_func;

  PL_RETURN_IF_ERROR(SetParent(parent_node));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, agg_func_));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(parent(), this));
  return Status();
}

bool BlockingAggIR::HasLogicalRepr() const { return true; }

std::string BlockingAggIR::DebugString(int64_t depth) const {
  return DebugStringFmt(depth, absl::StrFormat("%d:BlockingAggIR", id()),
                        {{"Parent", parent()->DebugString(depth + 1)},
                         {"ByFn", by_func_->DebugString(depth + 1)},
                         {"AggFn", agg_func_->DebugString(depth + 1)}});
}

Status BlockingAggIR::EvaluateAggregateExpression(carnotpb::AggregateExpression* expr,
                                                  const IRNode& ir_node) const {
  DCHECK(ir_node.type() == IRNodeType::FuncType);
  auto casted_ir = static_cast<const FuncIR&>(ir_node);
  expr->set_name(casted_ir.func_name());
  for (auto ir_arg : casted_ir.args()) {
    auto arg_pb = expr->add_args();
    switch (ir_arg->type()) {
      case IRNodeType::ColumnType: {
        auto col = arg_pb->mutable_column();
        col->set_node(parent()->id());
        col->set_index(static_cast<ColumnIR*>(ir_arg)->col_idx());
        break;
      }
      case IRNodeType::IntType: {
        auto value = arg_pb->mutable_constant();
        auto casted_ir = static_cast<IntIR*>(ir_arg);
        value->set_data_type(types::DataType::INT64);
        value->set_int64_value(casted_ir->val());
        break;
      }
      case IRNodeType::StringType: {
        auto value = arg_pb->mutable_constant();
        auto casted_ir = static_cast<StringIR*>(ir_arg);
        value->set_data_type(types::DataType::STRING);
        value->set_string_value(casted_ir->str());
        break;
      }
      case IRNodeType::FloatType: {
        auto value = arg_pb->mutable_constant();
        auto casted_ir = static_cast<FloatIR*>(ir_arg);
        value->set_data_type(types::DataType::FLOAT64);
        value->set_float64_value(casted_ir->val());
        break;
      }
      case IRNodeType::BoolType: {
        auto value = arg_pb->mutable_constant();
        auto casted_ir = static_cast<BoolIR*>(ir_arg);
        value->set_data_type(types::DataType::BOOLEAN);
        value->set_bool_value(casted_ir->val());
        break;
      }
      case IRNodeType::TimeType: {
        auto value = arg_pb->mutable_constant();
        auto casted_ir = static_cast<const TimeIR&>(ir_node);
        value->set_data_type(types::DataType::TIME64NS);
        value->set_time64_ns_value(static_cast<::google::protobuf::int64>(casted_ir.val()));
        break;
      }
      default: {
        return error::InvalidArgument("Didn't expect node of type $0 in expression evaluator.",
                                      ir_node.type_string());
      }
    }
  }
  return Status::OK();
}

Status BlockingAggIR::ToProto(carnotpb::Operator* op) const {
  auto pb = new carnotpb::BlockingAggregateOperator();

  for (const auto& agg_expr : agg_val_vector_) {
    auto expr = pb->add_values();
    PL_RETURN_IF_ERROR(EvaluateAggregateExpression(expr, *agg_expr.node));
    pb->add_value_names(agg_expr.name);
  }

  if (by_func_ != nullptr) {
    for (const auto& group : groups_) {
      auto group_pb = pb->add_groups();
      group_pb->set_node(parent()->id());
      group_pb->set_index(group->col_idx());
      pb->add_group_names(group->col_name());
    }
  }

  op->set_op_type(carnotpb::BLOCKING_AGGREGATE_OPERATOR);
  op->set_allocated_blocking_agg_op(pb);
  return Status::OK();
}

bool ColumnIR::HasLogicalRepr() const { return false; }
Status ColumnIR::Init(const std::string& col_name, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  col_name_ = col_name;
  return Status::OK();
}

std::string ColumnIR::DebugString(int64_t depth) const {
  return absl::StrFormat("%s%d:%s\t-\t%s", std::string(depth, '\t'), id(), "Column", col_name());
}

bool StringIR::HasLogicalRepr() const { return false; }
Status StringIR::Init(std::string str, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  str_ = str;
  return Status::OK();
}

std::string StringIR::DebugString(int64_t depth) const {
  return absl::StrFormat("%s%d:%s\t-\t%s", std::string(depth, '\t'), id(), "Str", str());
}

bool ListIR::HasLogicalRepr() const { return false; }
Status ListIR::Init(const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  return Status::OK();
}
Status ListIR::AddListItem(IRNode* node) {
  children_.push_back(node);
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, node));
  return Status::OK();
}
std::string ListIR::DebugString(int64_t depth) const {
  std::map<std::string, std::string> childMap;
  for (size_t i = 0; i < children_.size(); i++) {
    childMap[absl::StrFormat("child%d", i)] = children_[i]->DebugString(depth + 1);
  }
  return DebugStringFmt(depth, absl::StrFormat("%d:ListIR", id()), childMap);
}

bool LambdaIR::HasLogicalRepr() const { return false; }
bool LambdaIR::HasDictBody() const { return has_dict_body_; }

Status LambdaIR::Init(std::unordered_set<std::string> expected_column_names,
                      const ColExpressionVector& col_exprs, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  expected_column_names_ = expected_column_names;
  col_exprs_ = col_exprs;
  has_dict_body_ = true;
  return Status::OK();
}

Status LambdaIR::Init(std::unordered_set<std::string> expected_column_names, IRNode* node,
                      const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  expected_column_names_ = expected_column_names;
  col_exprs_.push_back(ColumnExpression{"default_key", node});
  has_dict_body_ = false;
  return Status::OK();
}

StatusOr<IRNode*> LambdaIR::GetDefaultExpr() {
  if (HasDictBody()) {
    return error::InvalidArgument(
        "Couldn't return the default expression, Lambda initialized as dict.");
  }
  for (const auto& col_expr : col_exprs_) {
    if (col_expr.name == "default_key") {
      return col_expr.node;
    }
  }
  return error::InvalidArgument(
      "Couldn't return the default expression, no default expression in column expression vector.");
}

std::string LambdaIR::DebugString(int64_t depth) const {
  std::map<std::string, std::string> childMap;
  childMap["ExpectedRelation"] =
      absl::StrFormat("[%s]", absl::StrJoin(expected_column_names_, ","));
  for (auto const& x : col_exprs_) {
    childMap[absl::StrFormat("ExprMap[\"%s\"]", x.name)] = x.node->DebugString(depth + 1);
  }
  return DebugStringFmt(depth, absl::StrFormat("%d:LambdaIR", id()), childMap);
}

bool FuncIR::HasLogicalRepr() const { return false; }
Status FuncIR::Init(std::string func_name, const std::vector<IRNode*>& args,
                    const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  func_name_ = func_name;
  args_ = args;
  for (auto a : args_) {
    if (a == nullptr) {
      return error::Internal("Argument for FuncIR is null.");
    }
  }
  return Status::OK();
}

std::string FuncIR::DebugString(int64_t depth) const {
  std::map<std::string, std::string> childMap;
  for (size_t i = 0; i < args_.size(); i++) {
    childMap.emplace(absl::StrFormat("arg%d", i), args_[i]->DebugString(depth + 1));
  }
  return DebugStringFmt(depth, absl::StrFormat("%d:FuncIR", id()), childMap);
}

/* Float IR */
bool FloatIR::HasLogicalRepr() const { return false; }
Status FloatIR::Init(double val, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  val_ = val;
  return Status::OK();
}
std::string FloatIR::DebugString(int64_t depth) const {
  return absl::StrFormat("%s%d:%s\t-\t%f", std::string(depth, '\t'), id(), "Float", val());
}

/* Int IR */
bool IntIR::HasLogicalRepr() const { return false; }
Status IntIR::Init(int64_t val, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  val_ = val;
  return Status::OK();
}
std::string IntIR::DebugString(int64_t depth) const {
  return absl::StrFormat("%s%d:%s\t-\t%d", std::string(depth, '\t'), id(), "Int", val());
}

/* Bool IR */
bool BoolIR::HasLogicalRepr() const { return false; }
Status BoolIR::Init(bool val, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  val_ = val;
  return Status::OK();
}
std::string BoolIR::DebugString(int64_t depth) const {
  return absl::StrFormat("%s%d:%s\t-\t%d", std::string(depth, '\t'), id(), "Bool", val());
}
/* Time IR */
bool TimeIR::HasLogicalRepr() const { return false; }
Status TimeIR::Init(int64_t val, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  val_ = val;
  return Status::OK();
}
std::string TimeIR::DebugString(int64_t depth) const {
  return absl::StrFormat("%s%d:%s\t-\t%d", std::string(depth, '\t'), id(), "Time", val());
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
