#pragma once

#include <arrow/memory_pool.h>
#include <sole.hpp>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/udf/registry.h"
#include "src/carnotpb/carnot.pb.h"
#include "src/common/base/base.h"
#include "src/table_store/table/table_store.h"

PL_SUPPRESS_WARNINGS_START()
#include "src/carnotpb/carnot.grpc.pb.h"
// TODO(michelle/nserrino): Fix this so that we don't need to the NOLINT.
// NOLINTNEXTLINE(build/include_subdir)
#include "blockingconcurrentqueue.h"
PL_SUPPRESS_WARNINGS_END()

namespace pl {
namespace md {
class AgentMetadataState;
}
namespace carnot {
namespace exec {

/**
 * ExecState manages the execution state for a single query. A new one will
 * be constructed for every query executed in Carnot and it will not be reused.
 *
 * The purpose of this class is to keep track of resources required for the query
 * and provide common resources (UDFs, UDA, etc) the operators within the query.
 */
using table_store::TableStore;
using KelvinStubGenerator = std::function<std::unique_ptr<carnotpb::KelvinService::StubInterface>(
    const std::string& address)>;

class ExecState {
 public:
  explicit ExecState(udf::ScalarUDFRegistry* scalar_udf_registry, udf::UDARegistry* uda_registry,
                     std::shared_ptr<TableStore> table_store,
                     const KelvinStubGenerator& stub_generator, const sole::uuid& query_id)
      : scalar_udf_registry_(scalar_udf_registry),
        uda_registry_(uda_registry),
        table_store_(std::move(table_store)),
        stub_generator_(stub_generator),
        query_id_(query_id) {}
  arrow::MemoryPool* exec_mem_pool() {
    // TOOD(zasgar): Make this the correct pool.
    return arrow::default_memory_pool();
  }

  udf::ScalarUDFRegistry* scalar_udf_registry() { return scalar_udf_registry_; }
  udf::UDARegistry* uda_registry() { return uda_registry_; }

  TableStore* table_store() { return table_store_.get(); }

  const sole::uuid& query_id() const { return query_id_; }

  Status AddScalarUDF(int64_t id, const std::string& name,
                      const std::vector<types::DataType> arg_types) {
    PL_ASSIGN_OR_RETURN(auto def, scalar_udf_registry_->GetDefinition(name, arg_types));
    id_to_scalar_udf_map_[id] = def;
    return Status::OK();
  }

  Status AddUDA(int64_t id, const std::string& name, const std::vector<types::DataType> arg_types) {
    PL_ASSIGN_OR_RETURN(auto def, uda_registry_->GetDefinition(name, arg_types));
    id_to_uda_map_[id] = def;
    return Status::OK();
  }

  // This function returns a stub to the Kelvin GRPC Service.
  std::unique_ptr<carnotpb::KelvinService::StubInterface> KelvinServiceStub(
      const std::string& remote_address) {
    return stub_generator_(remote_address);
  }

  udf::ScalarUDFDefinition* GetScalarUDFDefinition(int64_t id) { return id_to_scalar_udf_map_[id]; }

  std::map<int64_t, udf::ScalarUDFDefinition*> id_to_scalar_udf_map() {
    return id_to_scalar_udf_map_;
  }

  udf::UDADefinition* GetUDADefinition(int64_t id) { return id_to_uda_map_[id]; }

  std::unique_ptr<udf::FunctionContext> CreateFunctionContext() {
    auto ctx = std::make_unique<udf::FunctionContext>(metadata_state_);
    return ctx;
  }

  // A node can call this method to say no more records will be processed (ie. Limit).
  // That node is responsible for setting eos.
  void StopLimitReached() { keep_running_ = false; }

  bool keep_running() { return keep_running_; }

  void set_metadata_state(std::shared_ptr<const md::AgentMetadataState> metadata_state) {
    metadata_state_ = metadata_state;
  }

 private:
  udf::ScalarUDFRegistry* scalar_udf_registry_;
  udf::UDARegistry* uda_registry_;
  std::shared_ptr<TableStore> table_store_;
  std::shared_ptr<const md::AgentMetadataState> metadata_state_;
  const KelvinStubGenerator stub_generator_;
  std::map<int64_t, udf::ScalarUDFDefinition*> id_to_scalar_udf_map_;
  std::map<int64_t, udf::UDADefinition*> id_to_uda_map_;
  const sole::uuid query_id_;
  bool keep_running_ = true;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
