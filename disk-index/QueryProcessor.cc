/*
 * Copyright ©2025 Hal Perkins.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Spring Quarter 2025 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include "./QueryProcessor.h"

#include <iostream>
#include <algorithm>
#include <list>
#include <string>
#include <vector>
#include <unordered_set>

extern "C" {
  #include "./libhw1/CSE333.h"
}

using std::list;
using std::sort;
using std::string;
using std::vector;
using std::unordered_set;

namespace hw3 {

// Adds all docs from all indices that contain first query word as results to
// final_result
// Args:
// - first_word: first query word to look for
// - array_len: num indices
// - itr_arr: array of pointers to IndexTableReader objects,
//            which map from word to docID table
// - dtr_arr: array of pointers to DocTableReader objects,
//            which map from docID to list of positions
//            (has function to give doc name)
// - final_result: (output param) will contain QueryResults for document names
//                 containing first_word
//                 with its number of occurrences in the doc
static void AddFirstWordDocs(const string first_word, const int& array_len,
                             IndexTableReader** itr_arr,
                             DocTableReader** dtr_arr,
                             vector<QueryProcessor::QueryResult>*
                              final_result);

// For each QueryResult in final_result,
//   if its doc contains word, increment its rank with
//   word's number of occurences in doc
//   Otherwise, remove from final_result
// Args:
// - word: query word to update for
// - array_len: num indices
// - itr_arr: array of pointers to IndexTableReader objects,
//            which map from word to docID table
// - dtr_arr: array of pointers to DocTableReader objects,
//            which map from docID to list of positions
//            (has function to give doc name)
// - final_result: (output param) its starting QueryResults will either have
//                 incremented rank or be removed,
//                 according to what's described above
static void UpdateDocsForWord(const string word, const int& array_len,
                              IndexTableReader** itr_arr,
                              DocTableReader** dtr_arr,
                              vector<QueryProcessor::QueryResult>*
                                final_result);

// Erases QueryResults that are not in results_with_word from final_result
// Args:
// - results_with_word: set of doc names for which
//                      their docs contain a particular word
// - final_result: (output param) filtered as described above
static void RemoveResultsWithoutWord(unordered_set<string> results_with_word,
                                    vector<QueryProcessor::QueryResult>*
                                      final_result);

QueryProcessor::QueryProcessor(const list<string> &index_list, bool validate) {
  // Stash away a copy of the index list.
  index_list_ = index_list;
  array_len_ = index_list_.size();
  Verify333(array_len_ > 0);

  // Create the arrays of DocTableReader*'s. and IndexTableReader*'s.
  dtr_array_ = new DocTableReader*[array_len_];
  itr_array_ = new IndexTableReader*[array_len_];

  // Populate the arrays with heap-allocated DocTableReader and
  // IndexTableReader object instances.
  list<string>::const_iterator idx_iterator = index_list_.begin();
  for (int i = 0; i < array_len_; i++) {
    FileIndexReader fir(*idx_iterator, validate);
    dtr_array_[i] = fir.NewDocTableReader();
    itr_array_[i] = fir.NewIndexTableReader();
    idx_iterator++;
  }
}

QueryProcessor::~QueryProcessor() {
  // Delete the heap-allocated DocTableReader and IndexTableReader
  // object instances.
  Verify333(dtr_array_ != nullptr);
  Verify333(itr_array_ != nullptr);
  for (int i = 0; i < array_len_; i++) {
    delete dtr_array_[i];
    delete itr_array_[i];
  }

  // Delete the arrays of DocTableReader*'s and IndexTableReader*'s.
  delete[] dtr_array_;
  delete[] itr_array_;
  dtr_array_ = nullptr;
  itr_array_ = nullptr;
}

// This structure is used to store a index-file-specific query result.
typedef struct {
  DocID_t doc_id;  // The document ID within the index file.
  int rank;        // The rank of the result so far.
} IdxQueryResult;

vector<QueryProcessor::QueryResult>
QueryProcessor::ProcessQuery(const vector<string> &query) const {
  Verify333(query.size() > 0);

  // STEP 1.
  // (the only step in this file)
  vector<QueryProcessor::QueryResult> final_result;  // (given)

  const string first_word = query.front();
  AddFirstWordDocs(first_word, array_len_, itr_array_, dtr_array_,
                   &final_result);

  // for ea remaining word
  for (int i = 1; i < static_cast<int>(query.size()); i++) {
    const string curr_word = query[i];
    // if word in doc, incr doc's rank
    // else, remove doc
    UpdateDocsForWord(curr_word, array_len_, itr_array_, dtr_array_,
                      &final_result);

     // ith word wasn't found in any result doc? return empty
    if (final_result.empty()) return final_result;
  }


  // Sort the final results.
  sort(final_result.begin(), final_result.end());
  return final_result;
}

static void AddFirstWordDocs(const string first_word, const int& array_len,
                             IndexTableReader** itr_arr,
                             DocTableReader** dtr_arr,
                             vector<QueryProcessor::QueryResult>*
                             final_result) {
  // go through ea index
  for (int i = 0; i < array_len; i++) {
    // maps from a word to an embedded docID hash table, or docID table
    IndexTableReader* curr_itr = itr_arr[i];
    // maps from a 64-bit docID to a list of positions
    DocIDTableReader* id_to_pos = curr_itr->LookupWord(first_word);

    // if word found in curr index
    if (id_to_pos != nullptr) {
      // add matching docs as potential query results

      // docid_to_docname HashTable
      DocTableReader* curr_dtr = dtr_arr[i];
      // header has doc id and doc's num positions
      const list<DocIDElementHeader> headers = id_to_pos->GetDocIDList();
      for (const DocIDElementHeader& header : headers) {
        QueryProcessor::QueryResult result;

        string doc_name;
        Verify333(curr_dtr->LookupDocID(header.doc_id, &doc_name));
        result.document_name = doc_name;

        // (the initial computed rank is the number of times the word
        // appears in that document).
        result.rank = header.num_positions;

        final_result->push_back(result);
      }  // end of loop over ith index's docs for first word
    }
    delete id_to_pos;
  }  // end of loop over ea index for first word
}

static void UpdateDocsForWord(const string word, const int& array_len,
                              IndexTableReader** itr_arr,
                              DocTableReader** dtr_arr,
                              vector<QueryProcessor::QueryResult>*
                                final_result) {
  bool found_ith_word = false;
  unordered_set<string> results_with_ith_word;
  // go through ea index
  for (int j = 0; j < array_len; j++) {
    IndexTableReader* curr_itr = itr_arr[j];
    DocIDTableReader* id_to_pos = curr_itr->LookupWord(word);
    // if curr word found in curr index (lookup)
    if (id_to_pos != nullptr) {
      // go through curr index's docs for curr word
      DocTableReader* curr_dtr = dtr_arr[j];
      const list<DocIDElementHeader> headers = id_to_pos->GetDocIDList();
      for (const DocIDElementHeader& header : headers) {
        string doc_name;
        Verify333(curr_dtr->LookupDocID(header.doc_id, &doc_name));
        // if doc is already in final_result, increment rank
        for (QueryProcessor::QueryResult& result : *(final_result)) {
          if (result.document_name == doc_name) {
            found_ith_word = true;
            results_with_ith_word.insert(result.document_name);
            result.rank += header.num_positions;
            break;
          }
        }  // end of loop over final_result docs
      }  // end of loop over jth index's docs for ith word
    }
    delete id_to_pos;
  }  // end of loop over ea index for ith word
  // word not found in any result doc? return empty
  if (!found_ith_word) {
    final_result->clear();  // deletes ea QueryResult elem
  }
  // result doc not incremented for ith word? remove from final_result
  RemoveResultsWithoutWord(results_with_ith_word, final_result);
  final_result->shrink_to_fit();
}

static void RemoveResultsWithoutWord(unordered_set<string> results_with_word,
                                     vector<QueryProcessor::QueryResult>*
                                        final_result) {
  final_result->erase(
    // first, last iterators defining range of elems to erase

    // remove_if() reorders vector so
    // elems to keep are at front, elems to remove at end
    // returns iterator pointing to elems to remove
    remove_if(final_result->begin(), final_result->end(),
      [&results_with_word](const QueryProcessor::QueryResult &result) {
        // remove if didn't find result in results with ith word
        return results_with_word.find(result.document_name)
               == results_with_word.end();
      }),
    final_result->end());
}

}  // namespace hw3
