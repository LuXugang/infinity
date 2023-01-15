//
// Created by JinHai on 2022/11/12.
//

#include "data_block.h"

namespace infinity {

void
DataBlock::Init(const std::vector<DataType> &types) {
    StorageAssert(!initialized, "Data block was initialized before.");
    column_count_ = types.size();
    column_vectors.reserve(column_count_);
    for(SizeT i = 0; i < column_count_; ++ i) {
        column_vectors.emplace_back(MakeShared<ColumnVector>(types[i]));
        column_vectors[i]->Initialize();
    }
    initialized = true;
}

void
DataBlock::Init(const Vector<SharedPtr<ColumnVector>>& input_vectors) {
    column_count_ = input_vectors.size();
    column_vectors = input_vectors;
    initialized = true;
    Finalize();
}

void
DataBlock::Reset() {

    // Reset behavior:
    // Reset each column into just initialized status.
    // No data is appended into any column.

    for(SizeT i = 0; i < column_count_; ++ i) {
        column_vectors[i]->Reset();
        column_vectors[i]->Initialize();
    }

    row_count_ = 0;
}

Value
DataBlock::GetValue(SizeT column_index, SizeT row_index) const {
    return column_vectors[column_index]->GetValue(row_index);
}

void
DataBlock::SetValue(SizeT column_index, SizeT row_index, const Value &val) {
    StorageAssert(column_index < column_count_,
                  "Attempt to access invalid column index: " + std::to_string(column_index));
    column_vectors[column_index]->SetValue(row_index, val);
}

void
DataBlock::AppendValue(SizeT column_index, const Value& value) {
    StorageAssert(column_index < column_count_,
                  "Attempt to access invalid column index: " + std::to_string(column_index) +
                  " in column count: " + std::to_string(column_count_));
    column_vectors[column_index]->AppendValue(value);
    finalized = false;
}

void
DataBlock::Finalize() {
    SizeT row_count = column_vectors[0]->Size();
    for(SizeT i = 1; i < column_count(); ++ i) {
        StorageAssert(column_vectors[i]->vector_type() == ColumnVectorType::kConstant
                      || row_count == column_vectors[i]->Size(),
                      "Column vectors in same data block have different size.")
    }
    row_count_ = row_count;
    finalized = true;
}

String
DataBlock::ToString() const {
    NotImplementError("Not implemented.")
}

}


