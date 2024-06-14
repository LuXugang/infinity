// Copyright(C) 2023 InfiniFlow, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

module;

#include "CControl_svd.h"
#include "inc/mlas.h"

module emvb_product_quantization;
import stl;
import mlas_matrix_multiply;
import kmeans_partition;
import vector_distance;
import index_base;
import third_party;
import logger;
import infinity_exception;

namespace infinity {

template <std::unsigned_integral SUBSPACE_CENTROID_TAG, u32 SUBSPACE_NUM>
void PQ<SUBSPACE_CENTROID_TAG, SUBSPACE_NUM>::EncodeEmbedding(const f32 *embedding_data, const u32 embedding_num, auto output_iter) const {
    const auto buffer = MakeUniqueForOverwrite<f32[]>(embedding_num * subspace_centroid_num_);
    const f32 *part_embedding = embedding_data; // size: embedding_num * subspace_dimension_, stride: dimension_
    for (u32 j = 0; j < SUBSPACE_NUM; ++j) {
        MlasGemm(CblasNoTrans,
                 CblasTrans,
                 embedding_num,
                 subspace_centroid_num_,
                 subspace_dimension_,
                 1.0f,
                 part_embedding,
                 dimension_,
                 subspace_centroids_[j].data(),
                 subspace_dimension_,
                 0.0f,
                 buffer.get(),
                 subspace_centroid_num_,
                 nullptr);
        part_embedding += subspace_dimension_;
        // find max id (for every embedding, find centroid with min l2 distance, and equivalently max (x*y - 0.5*y^2))
        const auto &c_norm_data = subspace_centroid_norms_neg_half[j];
        const f32 *buffer_ptr = buffer.get();
        auto copy_output_iter = output_iter;
        for (u32 i = 0; i < embedding_num; ++i) {
            f32 max_neg_distance = std::numeric_limits<f32>::lowest();
            u32 max_id = 0;
            for (u32 k = 0; k < subspace_centroid_num_; ++k) {
                if (const f32 neg_distance = buffer_ptr[k] + c_norm_data[k]; neg_distance > max_neg_distance) {
                    max_neg_distance = neg_distance;
                    max_id = k;
                }
            }
            buffer_ptr += subspace_centroid_num_;
            Array<SUBSPACE_CENTROID_TAG, SUBSPACE_NUM> &encoded_target = *copy_output_iter;
            ++copy_output_iter;
            encoded_target[j] = max_id;
        }
    }
}

template <std::unsigned_integral SUBSPACE_CENTROID_TAG, u32 SUBSPACE_NUM>
UniquePtr<f32[]> PQ<SUBSPACE_CENTROID_TAG, SUBSPACE_NUM>::DecodeEmbedding(auto input_iter_begin, const u32 embedding_num) const {
    auto result = MakeUniqueForOverwrite<f32[]>(embedding_num * dimension_);
    auto output_ptr = result.get();
    for (u32 i = 0; i < embedding_num; ++i) {
        const Array<SUBSPACE_CENTROID_TAG, SUBSPACE_NUM> &encoded_embedding = *input_iter_begin;
        for (u32 j = 0; j < SUBSPACE_NUM; ++j) {
            const auto src_from = subspace_centroids_[j].data() + encoded_embedding[j] * subspace_dimension_;
            std::copy_n(src_from, subspace_dimension_, output_ptr);
            output_ptr += subspace_dimension_;
        }
        ++input_iter_begin;
    }
    return result;
}

template <std::unsigned_integral SUBSPACE_CENTROID_TAG, u32 SUBSPACE_NUM>
PQ<SUBSPACE_CENTROID_TAG, SUBSPACE_NUM>::PQ(const u32 subspace_dimension) : subspace_dimension_(subspace_dimension) {
    const u32 subspace_centroid_data_size = subspace_centroid_num_ * subspace_dimension_;
    for (auto &centroids_v : subspace_centroids_) {
        centroids_v.resize(subspace_centroid_data_size);
    }
}

template <std::unsigned_integral SUBSPACE_CENTROID_TAG, u32 SUBSPACE_NUM>
OPQ<SUBSPACE_CENTROID_TAG, SUBSPACE_NUM>::OPQ(const u32 subspace_dimension) : PQ<SUBSPACE_CENTROID_TAG, SUBSPACE_NUM>(subspace_dimension) {
    matrix_R_ = MakeUniqueForOverwrite<f32[]>(this->dimension_ * this->dimension_);
}

template <std::unsigned_integral SUBSPACE_CENTROID_TAG, u32 SUBSPACE_NUM>
void PQ<SUBSPACE_CENTROID_TAG, SUBSPACE_NUM>::Train(const f32 *embedding_data, const u32 embedding_num, const u32 iter_cnt) {
    const auto part_train_data = MakeUniqueForOverwrite<f32[]>(embedding_num * subspace_dimension_);
    for (u32 i = 0; i < SUBSPACE_NUM; ++i) {
        // copy subspace data
        const f32 *copy_src = embedding_data + i * subspace_dimension_;
        f32 *copy_dst = part_train_data.get();
        for (u32 j = 0; j < embedding_num; ++j) {
            std::copy_n(copy_src, subspace_dimension_, copy_dst);
            copy_src += this->dimension_;
            copy_dst += subspace_dimension_;
        }
        // k-means
        const auto centroid_cnt_result = GetKMeansCentroids<f32>(MetricType::kMetricL2,
                                                                 this->subspace_dimension_,
                                                                 embedding_num,
                                                                 part_train_data.get(),
                                                                 subspace_centroids_[i],
                                                                 subspace_centroid_num_,
                                                                 iter_cnt);
        if (centroid_cnt_result != subspace_centroid_num_) {
            const auto error_info = fmt::format("KMeans failed to find {} centroids for subspace", subspace_centroid_num_);
            LOG_ERROR(error_info);
            UnrecoverableError(error_info);
        }
        // compute norms
        auto &norms = this->subspace_centroid_norms_neg_half[i];
        const f32 *centroid_data = subspace_centroids_[i].data();
        for (u32 j = 0; j < subspace_centroid_num_; ++j) {
            norms[j] = -0.5f * L2NormSquare<f32, f32, u32>(centroid_data, subspace_dimension_);
            centroid_data += subspace_dimension_;
        }
    }
}

template <std::unsigned_integral SUBSPACE_CENTROID_TAG, u32 SUBSPACE_NUM>
void OPQ<SUBSPACE_CENTROID_TAG, SUBSPACE_NUM>::Train(const f32 *embedding_data, const u32 embedding_num, const u32 iter_cnt) {
    // step 1. set R to I
    std::fill_n(matrix_R_.get(), this->dimension_ * this->dimension_, 0.0f);
    for (u32 i = 0; i < this->dimension_; ++i) {
        matrix_R_[i * this->dimension_ + i] = 1.0f;
    }
    // step 2. train R for iter_cnt times
    const auto transformed_embedding = MakeUniqueForOverwrite<f32[]>(embedding_num * this->dimension_);
    const auto encoded_transformed = MakeUniqueForOverwrite<Array<SUBSPACE_CENTROID_TAG, SUBSPACE_NUM>[]>(embedding_num);
    const auto square_for_svd = MakeUniqueForOverwrite<f32[]>(this->dimension_ * this->dimension_);
    const auto svd_u = MakeUniqueForOverwrite<f32[]>(this->dimension_ * this->dimension_);
    const auto svd_s = MakeUniqueForOverwrite<f32[]>(this->dimension_);
    const auto svd_v = MakeUniqueForOverwrite<f32[]>(this->dimension_ * this->dimension_);
    for (u32 iter = iter_cnt; iter > 0; --iter) {
        matrixA_multiply_matrixB_output_to_C(embedding_data,
                                             matrix_R_.get(),
                                             embedding_num,
                                             this->dimension_,
                                             this->dimension_,
                                             transformed_embedding.get());
        if (iter == 1) {
            // final loop
            PQ_BASE::Train(transformed_embedding.get(), embedding_num, iter_cnt);
            return; // no update to R
        }
        // not the final loop
        const u32 train_iter_cnt = 1 + ((iter == 2) ? (iter_cnt / 2) : 0);
        PQ_BASE::Train(transformed_embedding.get(), embedding_num, train_iter_cnt);
        // update R
        PQ_BASE::EncodeEmbedding(transformed_embedding.get(), embedding_num, encoded_transformed.get());
        const auto decoded_encoded = PQ_BASE::DecodeEmbedding(encoded_transformed.get(), embedding_num); // embedding_num * dimension_
        transpose_matrixA_multiply_matrixB_output_to_C(embedding_data,
                                                       decoded_encoded.get(),
                                                       this->dimension_,
                                                       this->dimension_,
                                                       embedding_num,
                                                       square_for_svd.get());
        // TODO:svd
        svd(square_for_svd.get(), this->dimension_, this->dimension_, svd_u.get(), svd_s.get(), svd_v.get());
        // TODO: VT?
        matrixA_multiply_transpose_matrixB_output_to_C(svd_u.get(),
                                                       svd_v.get(),
                                                       this->dimension_,
                                                       this->dimension_,
                                                       this->dimension_,
                                                       matrix_R_.get());
    }
}

template <std::unsigned_integral SUBSPACE_CENTROID_TAG, u32 SUBSPACE_NUM>
void OPQ<SUBSPACE_CENTROID_TAG, SUBSPACE_NUM>::AddEmbeddings(const f32 *embedding_data, const u32 embedding_num) {
    // step 1. rotate by R
    const auto input_buffer = MakeUniqueForOverwrite<f32[]>(embedding_num * this->dimension_);
    matrixA_multiply_matrixB_output_to_C(embedding_data, matrix_R_.get(), embedding_num, this->dimension_, this->dimension_, input_buffer.get());
    // step 2. use PQ encoder
    std::lock_guard lock(this->write_mutex_);
    const u32 old_embedding_cnt = this->next_embedding_id_;
    const u32 next_end = old_embedding_cnt + embedding_num;
    this->next_embedding_id_ = next_end;
    this->encoded_embedding_data_.resize(next_end);
    auto encoded_embedding_output_iter = this->encoded_embedding_data_.begin() + old_embedding_cnt;
    PQ_BASE::EncodeEmbedding(input_buffer.get(), embedding_num, encoded_embedding_output_iter);
}

template <std::unsigned_integral SUBSPACE_CENTROID_TAG, u32 SUBSPACE_NUM>
UniquePtr<f32[]> OPQ<SUBSPACE_CENTROID_TAG, SUBSPACE_NUM>::GetIPDistanceTable(const f32 *query_data, const u32 query_num) const {
    auto result = MakeUniqueForOverwrite<f32[]>(SUBSPACE_NUM * this->subspace_centroid_num_ * query_num);
    // step 1. rotate by R
    const auto q_buffer = MakeUniqueForOverwrite<f32[]>(this->dimension_ * query_num);
    transpose_matrixA_multiply_transpose_matrixB_output_to_C(matrix_R_.get(),
                                                             query_data,
                                                             this->dimension_,
                                                             query_num,
                                                             this->dimension_,
                                                             q_buffer.get());
    // step 2. compute the inner products
    const f32 *subspace_query_ptr = q_buffer.get();
    const u32 subspace_query_stride = this->subspace_dimension_ * query_num;
    f32 *output_ptr = result.get();
    const u32 output_stride = this->subspace_centroid_num_ * query_num;
    for (u32 i = 0; i < SUBSPACE_NUM; ++i) {
        matrixA_multiply_matrixB_output_to_C(this->subspace_centroids_[i].data(),
                                             subspace_query_ptr,
                                             this->subspace_centroid_num_,
                                             query_num,
                                             this->subspace_dimension_,
                                             output_ptr);
        subspace_query_ptr += subspace_query_stride;
        output_ptr += output_stride;
    }
    return result;
}

template <std::unsigned_integral SUBSPACE_CENTROID_TAG, u32 SUBSPACE_NUM>
f32 PQ<SUBSPACE_CENTROID_TAG, SUBSPACE_NUM>::GetSingleIPDistance(const u32 embedding_id,
                                                                 const u32 query_id,
                                                                 const u32 query_num,
                                                                 const f32 *ip_table) const {
    f32 result = 0.0f;
    ip_table += query_id;
    const u32 stride = subspace_centroid_num_ * query_num;
    for (const auto &encoded_embedding = encoded_embedding_data_[embedding_id]; const auto centroid_id : encoded_embedding) {
        result += ip_table[centroid_id * query_num];
        ip_table += stride;
    }
    return result;
}

template <std::unsigned_integral SUBSPACE_CENTROID_TAG, u32 SUBSPACE_NUM>
void PQ<SUBSPACE_CENTROID_TAG, SUBSPACE_NUM>::GetMultipleIPDistance(const u32 embedding_offset,
                                                                    const u32 embedding_num,
                                                                    const u32 query_id,
                                                                    const u32 query_num,
                                                                    const f32 *ip_table,
                                                                    f32 *output_ptr) const {
    ip_table += query_id;
    const u32 stride = subspace_centroid_num_ * query_num;
    for (u32 i = 0; i < embedding_num; ++i) {
        auto copy_ip_table = ip_table;
        f32 result_i = 0.0f;
        for (const auto &encoded_embedding = encoded_embedding_data_[embedding_offset + i]; const auto centroid_id : encoded_embedding) {
            result_i += copy_ip_table[centroid_id * query_num];
            copy_ip_table += stride;
        }
        *output_ptr = result_i;
        ++output_ptr;
    }
}

template class OPQ<u8, 2>;

template class OPQ<u8, 4>;

template class OPQ<u8, 8>;

template class OPQ<u8, 16>;

template class OPQ<u8, 32>;

template class OPQ<u8, 64>;

template class OPQ<u8, 128>;

constexpr u32 current_max_subspace_num = 128;

template <std::unsigned_integral T>
UniquePtr<EMVBProductQuantizer> GetEMVBOPQT_Helper(const u32 pq_subspace_num, const u32 subspace_dimension) {
    auto error_msg = fmt::format("requested pq_subspace_num {} bigger than max value: {}.", pq_subspace_num, current_max_subspace_num);
    error_msg += " Please Add instantiation of OPQ with a bigger SUBSPACE_NUM value.";
    LOG_ERROR(error_msg);
    return nullptr;
}

template <std::unsigned_integral T, u32 I, u32... J>
UniquePtr<EMVBProductQuantizer> GetEMVBOPQT_Helper(const u32 pq_subspace_num, const u32 subspace_dimension) {
    if (pq_subspace_num == I) {
        return MakeUnique<OPQ<T, I>>(subspace_dimension);
    }
    if (pq_subspace_num < I) {
        auto error_info = fmt::format("unsupported pq subspace num: {}", pq_subspace_num);
        error_info += " Please Add instantiation of OPQ with your SUBSPACE_NUM value.";
        LOG_ERROR(error_info);
        UnrecoverableError(error_info);
        return nullptr;
    }
    return GetEMVBOPQT_Helper<T, J...>(pq_subspace_num, subspace_dimension);
}

template <std::unsigned_integral T>
UniquePtr<EMVBProductQuantizer> GetEMVBOPQT(const u32 pq_subspace_num, const u32 subspace_dimension) {
    return GetEMVBOPQT_Helper<T, 2, 4, 8, 16, 32, 64, 128>(pq_subspace_num, subspace_dimension);
}

UniquePtr<EMVBProductQuantizer> GetEMVBOPQ(const u32 pq_subspace_num, const u32 pq_subspace_bits, const u32 embedding_dimension) {
    if (embedding_dimension % pq_subspace_num != 0) {
        const auto error_info = fmt::format("embedding dimension {} is not a multiple of subspace number {}", embedding_dimension, pq_subspace_num);
        LOG_ERROR(error_info);
        UnrecoverableError(error_info);
    }
    const u32 subspace_dimension = embedding_dimension / pq_subspace_num;
    if (pq_subspace_bits == 8) {
        return GetEMVBOPQT<u8>(pq_subspace_num, subspace_dimension);
    }
    if (pq_subspace_bits == 16) {
        return GetEMVBOPQT<u16>(pq_subspace_num, subspace_dimension);
    }
    {
        const auto error_info = fmt::format("unsupported pq subspace bits num: {}, now support: 8, 16.", pq_subspace_bits);
        LOG_ERROR(error_info);
        UnrecoverableError(error_info);
        return nullptr;
    }
}

} // namespace infinity