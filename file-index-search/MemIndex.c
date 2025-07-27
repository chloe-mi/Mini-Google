// Copyright (c) 2025 Chloë Cartier
// Note: starter code by Hal Perkins, finishing code by Chloë Cartier.
// cartierc@uw.edu
// 2224694

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

#include "./MemIndex.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "libhw1/CSE333.h"
#include "libhw1/HashTable.h"
#include "libhw1/LinkedList.h"


///////////////////////////////////////////////////////////////////////////////
// Constants and declarations of internal-only helpers
///////////////////////////////////////////////////////////////////////////////

// Happily, HashTables dynamically resize themselves, so we can start by
// allocating a small hashtable.
#define HASHTABLE_INITIAL_NUM_BUCKETS 2

// Finds query_word in index. Returns iterator over query_word's postings
// hash table (where key is doc id, and value is positions
// of the word in the doc.)
// Args:
// - index: MemIndex (hash table, where key is a word's hash, and value is the
//          word's WordPostings. (WordPostings includes the word and its
//          postings hash table.)
// - query_word: word to get postings iterator for.
// Returns:
// - pointer to iterator over postings hash table: if query_word is in index.
//   Caller is responsible for freeing iterator.
// - NULL: otherwise.
static HTIterator* GetPostingsIter(MemIndex* index, char* query_word);


// The following functions use the MI_ prefix instead of the MemIndex_ prefix,
// to indicate that they should not be considered part of the MemIndex's
// public API.

// Comparator usable by LinkedList_Sort(), which implements an increasing
// order by rank over SearchResult's.  If the caller is interested in a
// decreasing sort order, they should invert the return value.
static int MI_SearchResultComparator(LLPayload_t e1, LLPayload_t e2) {
  SearchResult *sr1 = (SearchResult*) e1;
  SearchResult *sr2 = (SearchResult*) e2;

  if (sr1->rank > sr2->rank) {
    return 1;
  } else if (sr1->rank < sr2->rank) {
    return -1;
  } else {
    return 0;
  }
}

// Deallocator usable by LinkedList_Free(), which frees a list of
// of DocPositionOffset_t.  Since these offsets are stored inline (ie, not
// malloc'ed), there is nothing to do in this function.
static void MI_NoOpFree(LLPayload_t ptr) { }

// Deallocator usable by HashTable_Free(), which frees a LinkedList of
// DocPositionOffset_t (ie, our posting list).  We use these LinkedLists
// in our WordPostings.
static void MI_PostingsFree(HTValue_t ptr) {
  LinkedList *list = (LinkedList*) ptr;
  LinkedList_Free(list, &MI_NoOpFree);
}

// Deallocator used by HashTable_Free(), which frees a WordPostings.  A
// MemIndex consists of a HashTable of WordPostings (ie, this is the top-level
// structure).
static void MI_ValueFree(HTValue_t ptr) {
  WordPostings *wp = (WordPostings*) ptr;

  free(wp->word);
  HashTable_Free(wp->postings, &MI_PostingsFree);

  free(wp);
}

///////////////////////////////////////////////////////////////////////////////
// MemIndex implementation
///////////////////////////////////////////////////////////////////////////////
MemIndex* MemIndex_Allocate(void) {
  HashTable *index = HashTable_Allocate(HASHTABLE_INITIAL_NUM_BUCKETS);
  Verify333(index != NULL);
  return index;
}

void MemIndex_Free(MemIndex *index) {
  HashTable_Free(index, &MI_ValueFree);
}

int MemIndex_NumWords(MemIndex *index) {
  return HashTable_NumElements(index);
}

void MemIndex_AddPostingList(MemIndex *index, char *word, DocID_t doc_id,
                             LinkedList *postings) {
  HTKey_t key = FNVHash64((unsigned char*) word, strlen(word));
  HTKeyValue_t mi_kv, postings_kv, unused;
  WordPostings *wp;

  // STEP 1.
  // Remove this early return.


  // First, we have to see if the passed-in word already exists in
  // the inverted index.
  if (!HashTable_Find(index, key, &mi_kv)) {
    // STEP 2.
    // No, this is the first time the inverted index has seen this word.  We
    // need to prepare and insert a new WordPostings structure. Malloc:
    wp = (WordPostings*)malloc(sizeof(WordPostings));
    Verify333(wp != NULL);
    //   (1) find existing memory or allocate new memory for the WordPostings'
    //       word field (hint: remember that this function takes ownership
    //       of the passed-in word).
    wp->word = word;
    //   (2) allocate a new hashtable for the WordPostings' docID->postings
    //       mapping.
    wp->postings = HashTable_Allocate(HASHTABLE_INITIAL_NUM_BUCKETS);
    Verify333(wp->postings != NULL);
    //   (3) insert the the new WordPostings into the inverted index (ie, into
    //       the "index" table).
    mi_kv.key = key;
    mi_kv.value = wp;
    HashTable_Insert(index, mi_kv, &unused);

  } else {
    // Yes, this word already exists in the inverted index.  There's no need
    // to insert it again.

    // Instead of allocating a new WordPostings, we'll use the one that's
    // already in the inverted index.
    wp = (WordPostings*) mi_kv.value;

    // Ensure we don't have hash collisions (two different words that hash to
    // the same key, which is very unlikely).
    Verify333(strcmp(wp->word, word) == 0);

    // Now we can free the word (since the caller gave us ownership of it).
    free(word);
  }

  // At this point, we have a WordPostings struct which represents the posting
  // list for this word.  Add this new document's postings to it.

  // Verify that this document is not already in the posting list.
  Verify333(!HashTable_Find(wp->postings, doc_id, &postings_kv));

  // STEP 3.
  // Insert a new entry into the wp->postings hash table.
  // The entry's key is this docID and the entry's value
  // is the "postings" (ie, word positions list) we were passed
  // as an argument.
  postings_kv.key = doc_id;
  postings_kv.value = postings;
  HashTable_Insert(wp->postings, postings_kv, &unused);
}

LinkedList* MemIndex_Search(MemIndex *index, char *query[], int query_len) {
  LinkedList *ret_list;
  HTKeyValue_t kv;
  WordPostings *wp;
  HTKey_t key;
  int i;

  // If the user provided us with an empty search query, return NULL
  // to indicate failure.
  if (query_len == 0) {
    return NULL;
  }

  // STEP 4.
  // The most interesting part of Part C starts here...!
  //
  // Look up the first query word (ie, query[0]) in the inverted index.  For
  // each document that matches, allocate and initialize a SearchResult
  // structure (the initial computed rank is the number of times the word
  // appears in that document).  Finally, append the SearchResult onto ret_list.

  HTIterator* postings_it= GetPostingsIter(index, query[0]);
  if (postings_it == NULL) return NULL;
  // found first word in index

  ret_list = LinkedList_Allocate();
  Verify333(ret_list != NULL);

  // for ea doc w first word,
  while (HTIterator_IsValid(postings_it)) {
    // get curr kv: doc id to positions
    HTKeyValue_t words_curr_kv;  // doc id to positions
    Verify333(HTIterator_Get(postings_it, &words_curr_kv));

    // allocate SearchResult structure
    SearchResult* result_doc = (SearchResult*)malloc(sizeof(SearchResult));
    Verify333(result_doc != NULL);
    // initialize search result w doc id and rank
    result_doc->doc_id = words_curr_kv.key;
    result_doc->rank =
      LinkedList_NumElements((LinkedList*)words_curr_kv.value);
                                                     // value: positions list

    // append search result to ret_list
    LinkedList_Push(ret_list, (HTValue_t)result_doc);

    HTIterator_Next(postings_it);
  }
  HTIterator_Free(postings_it);

  // Great; we have our search results for the first query
  // word.  If there is only one query word, we're done!
  // Sort the result list and return it to the caller.
  if (query_len == 1) {
    LinkedList_Sort(ret_list, false, &MI_SearchResultComparator);
    return ret_list;
  }

  // OK, there are additional query words.  Handle them one
  // at a time.
  for (i = 1; i < query_len; i++) {
    LLIterator *ll_it;
    int j, num_docs;

    // STEP 5.
    // Look up the next query word (query[i]) in the inverted index.
    // If there are no matches, it means the overall query
    // should return no documents, so free ret_list and return NULL.
    key = FNVHash64((unsigned char*)query[i], strlen(query[i]));  // word hash
    if (!HashTable_Find(index, key, &kv)) {
      LinkedList_Free(ret_list, (LLPayloadFreeFnPtr)free);
      return NULL;
    }

    // STEP 6. Make sure ea first word doc has all other query words.
    wp = kv.value;
    HashTable* curr_postings = wp->postings;  // worddpostings for ith word:
                                              // word to ht
                                              // (of doc ids to positions)

    // loop over first word's result doc list
    ll_it = LLIterator_Allocate(ret_list);
    Verify333(ll_it != NULL);
    num_docs = LinkedList_NumElements(ret_list);
    for (j = 0; j < num_docs; j++) {
      // get jth result doc's id
      SearchResult* result_doc;
      LLIterator_Get(ll_it, (LLPayload_t*)&result_doc);
      DocID_t result_doc_id = (result_doc)->doc_id;

      HTKeyValue_t temp;
      if (HashTable_Find(curr_postings, result_doc_id, &temp)) {
        // ith word in jth result doc
        int num_occurrences_word_in_doc = LinkedList_NumElements(temp.value);
                                                          // value: postings
        ((SearchResult*)result_doc)->rank += num_occurrences_word_in_doc;
        LLIterator_Next(ll_it);
      } else {
        LLIterator_Remove(ll_it, &free);
      }
    }  // end of loop over first word's search result docs
    LLIterator_Free(ll_it);


    // We've finished processing this current query word.  If there are no
    // documents left in our result list, free ret_list and return NULL.
    if (LinkedList_NumElements(ret_list) == 0) {
      LinkedList_Free(ret_list, (LLPayloadFreeFnPtr)free);
      return NULL;
    }
  }  // end of loop over query words

  // Sort the result list by rank and return it to the caller.
  LinkedList_Sort(ret_list, false, &MI_SearchResultComparator);
  return ret_list;
}

// In STEP 4 GetFirstWordResults
static HTIterator* GetPostingsIter(MemIndex* index, char* query_word) {
  // hash word, look for word in index
  HTKey_t key = FNVHash64((unsigned char*)query_word, strlen(query_word));
  HTKeyValue_t kv;
  if (HashTable_Find(index, key, &kv)) {
    // get worddpostings: word to ht (of doc ids to positions)
    WordPostings* wp = kv.value;

    // iterate over ht of doc ids to positions
    HTIterator* postings_it = HTIterator_Allocate(wp->postings);
    Verify333(postings_it != NULL);
    return postings_it;

  } else {
    return NULL;
  }
}
