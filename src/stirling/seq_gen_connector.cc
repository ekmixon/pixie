#include "src/stirling/seq_gen_connector.h"

namespace pl {
namespace stirling {

void SeqGenConnector::TransferDataImpl(uint32_t table_num,
                                       types::ColumnWrapperRecordBatch* record_batch) {
  std::uniform_int_distribution<uint32_t> num_rows_dist(num_rows_min_, num_rows_max_);
  uint32_t num_records = num_rows_dist(rng_);

  switch (table_num) {
    case 0:
      TransferDataTable0(num_records, record_batch);
      break;
    case 1:
      TransferDataTable1(num_records, record_batch);
      break;
    default:
      LOG(ERROR) << absl::StrFormat("Cannot handle the specified table_num %d", table_num);
      ASSERT_TRUE(false);
  }
}

void SeqGenConnector::TransferDataTable0(uint32_t num_records,
                                         types::ColumnWrapperRecordBatch* record_batch) {
  for (uint32_t irecord = 0; irecord < num_records; ++irecord) {
    RecordBuilder<&kSeq0Table> r(record_batch);
    r.Append<r.ColIndex("time_")>(table0_time_seq_());
    r.Append<r.ColIndex("x")>(table0_lin_seq_());
    r.Append<r.ColIndex("xmod10")>(table0_mod10_seq_());
    r.Append<r.ColIndex("xsquared")>(table0_square_seq_());
    r.Append<r.ColIndex("fibonnaci")>(table0_fib_seq_());
    r.Append<r.ColIndex("PIx")>(table0_pi_seq_());
  }
}

void SeqGenConnector::TransferDataTable1(uint32_t num_records,
                                         types::ColumnWrapperRecordBatch* record_batch) {
  for (uint32_t irecord = 0; irecord < num_records; ++irecord) {
    RecordBuilder<&kSeq1Table> r(record_batch);
    r.Append<r.ColIndex("time_")>(table1_time_seq_());
    r.Append<r.ColIndex("x")>(table1_lin_seq_());
    r.Append<r.ColIndex("xmod8")>(table1_mod8_seq_());
  }
}

}  // namespace stirling
}  // namespace pl
