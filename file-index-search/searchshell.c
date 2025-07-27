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

// Feature test macro for strtok_r (c.f., Linux Programming Interface p. 63)
#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <inttypes.h>  // uint64_t format specifier

#include "libhw1/CSE333.h"
#include "./CrawlFileTree.h"
#include "./DocTable.h"
#include "./MemIndex.h"

#define MAX_QUERY_LENGTH 1000

//////////////////////////////////////////////////////////////////////////////
// Helper function declarations, constants, etc
static void Usage(void);
// static void ProcessQueries(DocTable *dt, MemIndex *mi);
// static int GetNextLine(FILE *f, char **ret_str);

// SearchResult comparator function.
// Accepts 2 SearchResults as LLPayload_ts.
// Returns an integer that is:
//    -1  if r1 <  r2
//     0  if r1 == r2
//    +1  if pr1 > r2
static int CompareResults(LLPayload_t r1, LLPayload_t r2);

// Given a query string, pulls out its tokens (separated by  ,\n),
// puts tokens as elements into an array.
// Args:
// - query: string to process.
// - tokens: (output param) string array to fill with query's tokens
//  (length MAX_QUERY_LENGTH).
// - num_tokens: (output param) pointer to int (should start at 0),
//  ends as num tokens extracted.
static void SplitQuery(char* query, char** tokens, int* num_tokens);

// Turns each token character into its lowercase version
// (according to the conversion rules defined by the C locale).
// token is an output param.
static void ToLowerWord(char* token);

// Prints a list of SearchResults - their document's name and rank,
// based on some earlier query that yielded them.
// Args:
// - results: to print.
// - doc_table: maps ids and names of documents in an index that
//  contains the SearchResults' documents.
static void PrintResults(LinkedList* results, DocTable* doc_table);

//////////////////////////////////////////////////////////////////////////////
// Implement searchshell!  We're giving you very few hints
// on how to do it, so you'll need to figure out an appropriate
// decomposition into functions as well as implementing the
// functions.  There are several major tasks you need to build:
//
//  - Crawl from a directory provided by argv[1] to produce and index
//  - Prompt the user for a query and read the query from stdin, in a loop
//  - Split a query into words (check out strtok_r)
//  - Process a query against the index and print out the results
//
// When searchshell detects end-of-file on stdin (cntrl-D from the
// keyboard), searchshell should free all dynamically allocated
// memory and any other allocated resources and then exit.
//
// Note that you should make sure the fomatting of your
// searchshell output exactly matches our solution binaries
// to get full points on this part.

// Main
int main(int argc, char **argv) {
  if (argc != 2) {
    Usage();
  }

  //  - Crawl from a directory provided by argv[1] to produce and index
  DocTable* doc_table;
  MemIndex* index;
  if (!CrawlFileTree(argv[1], &doc_table, &index)) {
    fprintf(stderr, "crawl file tree failed");
    return EXIT_FAILURE;
  }
  printf("Indexing '%s'\n", argv[1]);

  //  - Prompt the user for a query and read the query from stdin, in a loop
  char query[MAX_QUERY_LENGTH];
  printf("enter query:\n");
  while (fgets(query, MAX_QUERY_LENGTH, stdin)) {
    //  - Split a query into words
    // (to reason about tokens size:
    //  consider case all tokens are len 1, separated by len 1 delimiters)
    char* tokens[MAX_QUERY_LENGTH / 2];
    int num_tokens = 0;
    SplitQuery(query, tokens, &num_tokens);
                   // tokens is char** since arr name is ptr to 1st elem

    //  - Process a query against the index and print out the results
    LinkedList* results = MemIndex_Search(index, tokens, num_tokens);
    if (results == NULL) {  // no results
      printf("enter query:\n");
      continue;
    }

    LinkedList_Sort(results, false, &CompareResults);

    PrintResults(results, doc_table);

    LinkedList_Free(results, &free);
    printf("enter query:\n");
  }  // done querying
  printf("shutting down...\n");

  DocTable_Free(doc_table);
  MemIndex_Free(index);
  return EXIT_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
// Helper function definitions

static void Usage(void) {
  fprintf(stderr, "Usage: ./searchshell <docroot>\n");
  fprintf(stderr,
          "where <docroot> is an absolute or relative " \
          "path to a directory to build an index under.\n");
  exit(EXIT_FAILURE);
}

static int CompareResults(LLPayload_t r1, LLPayload_t r2) {
  return ((SearchResult*)r1)->rank - ((SearchResult*)r2)->rank;
}

static void SplitQuery(char* query, char** tokens, int* num_tokens) {
  *num_tokens = 0;
  char* save_ptr;
  char* token = strtok_r(query, " ,\n", &save_ptr);
  while (token != NULL && *num_tokens < MAX_QUERY_LENGTH / 2) {
    ToLowerWord(token);
    (*num_tokens)++;
    tokens[*num_tokens - 1] = token;  // 0-based indexing

    token = strtok_r(NULL, " ,\n", &save_ptr);
  }
}

static void ToLowerWord(char* token) {
  for (int i = 0; token[i]; i++) {
    token[i] = tolower(token[i]);
  }
}

static void PrintResults(LinkedList* results, DocTable* doc_table) {
    LLIterator* results_it = LLIterator_Allocate(results);
    Verify333(results_it != NULL);

    int num_results = LinkedList_NumElements(results);
    for (int i = 0; i < num_results; i++) {
      LLPayload_t curr_result;
      LLIterator_Get(results_it, &curr_result);

      char* doc_name
        = DocTable_GetDocName(doc_table, ((SearchResult*)curr_result)->doc_id);

      printf("  %s (%d)\n", doc_name, ((SearchResult*)curr_result)->rank);

      LLIterator_Next(results_it);
    }  // done printing this query's results

    LLIterator_Free(results_it);
}
