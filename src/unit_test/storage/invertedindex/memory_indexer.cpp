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

#include "type/complex/row_id.h"
#include "type/logical_type.h"
#include "unit_test/base_test.h"
#include <iostream>
#include <unistd.h>
import stl;
import analyzer;
import analyzer_pool;
import memory_pool;
import pool_allocator;
import index_defines;
import posting_list_format;
import column_vector;
import data_type;
import value;
import memory_indexer;
import column_index_reader;
import posting_iterator;

using namespace infinity;

class MemoryIndexerTest : public BaseTest {
protected:
    MemoryPool byte_slice_pool_{};
    RecyclePool buffer_pool_{};
    ThreadPool thread_pool_{4};
    optionflag_t flag_{OPTION_FLAG_ALL};

public:
    struct ExpectedPosting {
        String term;
        Vector<RowID> doc_ids;
        Vector<u32> tfs;
    };

public:
    void SetUp() override {
        system("rm -rf /tmp/infinity/fulltext_tbl1_col1");
        system("mkdir -p /tmp/infinity/fulltext_tbl1_col1");
    }

    void TearDown() override {}
};

TEST_F(MemoryIndexerTest, Insert) {
    // https://en.wikipedia.org/wiki/Finite-state_transducer
    const char *paragraphs[] = {
        R"#(A finite-state transducer (FST) is a finite-state machine with two memory tapes, following the terminology for Turing machines: an input tape and an output tape. This contrasts with an ordinary finite-state automaton, which has a single tape. An FST is a type of finite-state automaton (FSA) that maps between two sets of symbols.[1] An FST is more general than an FSA. An FSA defines a formal language by defining a set of accepted strings, while an FST defines a relation between sets of strings.)#",
        R"#(An FST will read a set of strings on the input tape and generates a set of relations on the output tape. An FST can be thought of as a translator or relater between strings in a set.)#",
        R"#(In morphological parsing, an example would be inputting a string of letters into the FST, the FST would then output a string of morphemes.)#",
        R"#(An automaton can be said to recognize a string if we view the content of its tape as input. In other words, the automaton computes a function that maps strings into the set {0,1}. Alternatively, we can say that an automaton generates strings, which means viewing its tape as an output tape. On this view, the automaton generates a formal language, which is a set of strings. The two views of automata are equivalent: the function that the automaton computes is precisely the indicator function of the set of strings it generates. The class of languages generated by finite automata is known as the class of regular languages.)#",
        R"#(The two tapes of a transducer are typically viewed as an input tape and an output tape. On this view, a transducer is said to transduce (i.e., translate) the contents of its input tape to its output tape, by accepting a string on its input tape and generating another string on its output tape. It may do so nondeterministically and it may produce more than one output for each input string. A transducer may also produce no output for a given input string, in which case it is said to reject the input. In general, a transducer computes a relation between two formal languages.)#",
    };
    const SizeT num_paragraph = sizeof(paragraphs) / sizeof(char *);
    SharedPtr<ColumnVector> column = ColumnVector::Make(MakeShared<DataType>(LogicalType::kVarchar));
    column->Initialize();
    for (SizeT i = 0; i < num_paragraph; ++i) {
        Value v = Value::MakeVarchar(String(paragraphs[i]));
        column->AppendValue(v);
    }
    Vector<ExpectedPosting> expected_postings = {{"fst", {0, 1, 2}, {4, 2, 2}}, {"automaton", {0, 3}, {2, 5}}, {"transducer", {0, 4}, {1, 4}}};

    MemoryIndexer
        indexer1("/tmp/infinity/fulltext_tbl1_col1", "chunk1", RowID(0U, 0U), flag_, "standard", byte_slice_pool_, buffer_pool_, thread_pool_);
    indexer1.Insert(column, 0, 1);
    indexer1.Insert(column, 1, 3);
    indexer1.Dump();

    MemoryIndexer
        indexer2("/tmp/infinity/fulltext_tbl1_col1", "chunk2", RowID(0U, 4U), flag_, "standard", byte_slice_pool_, buffer_pool_, thread_pool_);
    indexer2.Insert(column, 4, 1);
    while (indexer2.GetInflightTasks() > 0) {
        sleep(1);
        indexer2.CommitSync();
    }

    ColumnIndexReader reader;
    reader.Open("/tmp/infinity/fulltext_tbl1_col1", Vector<String>{"chunk1"}, Vector<RowID>{RowID(0U, 0U)}, flag_, &indexer2);
    for (SizeT i = 0; i < expected_postings.size(); ++i) {
        const ExpectedPosting &expected = expected_postings[i];
        const String &term = expected.term;

        UniquePtr<PostingIterator> post_iter(reader.Lookup(term, &byte_slice_pool_));
        ASSERT_TRUE(post_iter != nullptr);

        RowID doc_id = INVALID_ROWID;
        for (SizeT j = 0; j < expected.doc_ids.size(); ++j) {
            doc_id = post_iter->SeekDoc(expected.doc_ids[j]);
            ASSERT_EQ(doc_id, expected.doc_ids[j]);
            u32 tf = post_iter->GetCurrentTF();
            ASSERT_EQ(tf, expected.tfs[j]);
        }
        if (doc_id != INVALID_ROWID) {
            doc_id = post_iter->SeekDoc(doc_id + 1);
            ASSERT_EQ(doc_id, INVALID_ROWID);
        }
    }
}

TEST_F(MemoryIndexerTest, test2) {
    // https://en.wikipedia.org/wiki/Finite-state_transducer
    const char *paragraphs[] = {
        R"#(A finite-state transducer (FST) is a finite-state machine with two memory tapes, following the terminology for Turing machines: an input tape and an output tape. This contrasts with an ordinary finite-state automaton, which has a single tape. An FST is a type of finite-state automaton (FSA) that maps between two sets of symbols.[1] An FST is more general than an FSA. An FSA defines a formal language by defining a set of accepted strings, while an FST defines a relation between sets of strings.)#",
        R"#(An FST will read a set of strings on the input tape and generates a set of relations on the output tape. An FST can be thought of as a translator or relater between strings in a set.)#",
        R"#(In morphological parsing, an example would be inputting a string of letters into the FST, the FST would then output a string of morphemes.)#",
        R"#(An automaton can be said to recognize a string if we view the content of its tape as input. In other words, the automaton computes a function that maps strings into the set {0,1}. Alternatively, we can say that an automaton generates strings, which means viewing its tape as an output tape. On this view, the automaton generates a formal language, which is a set of strings. The two views of automata are equivalent: the function that the automaton computes is precisely the indicator function of the set of strings it generates. The class of languages generated by finite automata is known as the class of regular languages.)#",
        R"#(The two tapes of a transducer are typically viewed as an input tape and an output tape. On this view, a transducer is said to transduce (i.e., translate) the contents of its input tape to its output tape, by accepting a string on its input tape and generating another string on its output tape. It may do so nondeterministically and it may produce more than one output for each input string. A transducer may also produce no output for a given input string, in which case it is said to reject the input. In general, a transducer computes a relation between two formal languages.)#",
    };
    const SizeT num_paragraph = sizeof(paragraphs) / sizeof(char *);
    SharedPtr<ColumnVector> column = ColumnVector::Make(MakeShared<DataType>(LogicalType::kVarchar));
    column->Initialize();
    for (SizeT i = 0; i < num_paragraph; ++i) {
        Value v = Value::MakeVarchar(String(paragraphs[i]));
        column->AppendValue(v);
    }
    Vector<ExpectedPosting> expected_postings = {{"fst", {0, 1, 2}, {4, 2, 2}}, {"automaton", {0, 3}, {2, 5}}, {"transducer", {0, 4}, {1, 4}}};

    MemoryIndexer
        indexer1("/tmp/infinity/fulltext_tbl1_col1", "chunk1", RowID(0U, 0U), flag_, "standard", byte_slice_pool_, buffer_pool_, thread_pool_);
    indexer1.Insert(column, 0, 2, true);
    indexer1.Insert(column, 2, 2, true);
    indexer1.Insert(column, 4, 1, true);
    indexer1.Dump(true);

    ColumnIndexReader reader;
    reader.Open("/tmp/infinity/fulltext_tbl1_col1", Vector<String>{"chunk1"}, Vector<RowID>{RowID(0U, 0U)}, flag_, nullptr);
    for (SizeT i = 0; i < expected_postings.size(); ++i) {
        const ExpectedPosting &expected = expected_postings[i];
        const String &term = expected.term;

        UniquePtr<PostingIterator> post_iter(reader.Lookup(term, &byte_slice_pool_));
        ASSERT_TRUE(post_iter != nullptr);

        RowID doc_id = INVALID_ROWID;
        for (SizeT j = 0; j < expected.doc_ids.size(); ++j) {
            doc_id = post_iter->SeekDoc(expected.doc_ids[j]);
            ASSERT_EQ(doc_id, expected.doc_ids[j]);
            u32 tf = post_iter->GetCurrentTF();
            ASSERT_EQ(tf, expected.tfs[j]);
        }
        if (doc_id != INVALID_ROWID) {
            doc_id = post_iter->SeekDoc(doc_id + 1);
            ASSERT_EQ(doc_id, INVALID_ROWID);
        }
    }
}