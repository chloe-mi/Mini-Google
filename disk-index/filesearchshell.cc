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

#include <cstdlib>    // for EXIT_SUCCESS, EXIT_FAILURE
#include <iostream>   // for std::cout, std::cerr, etc.
#include <list>
#include <vector>
#include <string>
#include <sstream>

#include "./QueryProcessor.h"

using std::cerr;
using std::endl;

// Error usage message for the client to see
// Arguments:
// - prog_name: Name of the program
static void Usage(char *prog_name);

// Pulls tokens out of query, puts into query_words
// Args:
// - query: query to parse
// - query_words: (output param) vector, will hold tokens in order
static void SplitQuery(const std::string& query,
                       std::vector<std::string>* query_words);

// Turns each character in word to its lowercase version.
// Arg:
// - word: (output param) ptr to word to turn into lowercase version.
static void ToLowerWord(std::string* word);

// If results is empty, prints "  [no results]"
// Otherwise, prints each result in order on a new line like
// "  <doc name> (<rank>)"
// Arg:
// - results: query results to print
static void PrintResults(const std::vector<hw3::QueryProcessor::QueryResult>&
                         results);

// Your job is to implement the entire filesearchshell.cc
// functionality. We're essentially giving you a blank screen to work
// with; you need to figure out an appropriate design, to decompose
// the problem into multiple functions or classes if that will help,
// to pick good interfaces to those functions/classes, and to make
// sure that you don't leak any memory.
//
// Here are the requirements for a working solution:
//
// The user must be able to run the program using a command like:
//
//   ./filesearchshell ./foo.idx ./bar/baz.idx /tmp/blah.idx [etc]
//
// i.e., to pass a set of filenames of indices as command line
// arguments. Then, your program needs to implement a loop where
// each loop iteration it:
//
//  (a) prints to the console a prompt telling the user to input the
//      next query.
//
//  (b) reads a white-space separated list of query words from
//      std::cin, converts them to lowercase, and constructs
//      a vector of c++ strings out of them.
//
//  (c) uses QueryProcessor.cc/.h's QueryProcessor class to
//      process the query against the indices and get back a set of
//      query results.  Note that you should instantiate a single
//      QueryProcessor  object for the lifetime of the program, rather
//      than  instantiating a new one for every query.
//
//  (d) print the query results to std::cout in the format shown in
//      the transcript on the hw3 web page.
//
// Also, you're required to quit out of the loop when std::cin
// experiences EOF, which a user passes by pressing "control-D"
// on the console.  As well, users should be able to type in an
// arbitrarily long query -- you shouldn't assume anything about
// a maximum line length.  Finally, when you break out of the
// loop and quit the program, you need to make sure you deallocate
// all dynamically allocated memory.  We will be running valgrind
// on your filesearchshell implementation to verify there are no
// leaks or errors.
//
// You might find the following technique useful, but you aren't
// required to use it if you have a different way of getting the
// job done.  To split a std::string into a vector of words, you
// can use a std::stringstream to get the job done and the ">>"
// operator. See, for example, "gnomed"'s post on stackoverflow for
// their example on how to do this:
//
//   http://stackoverflow.com/questions/236129/c-how-to-split-a-string
//
// (Search for "gnomed" on that page. They use an istringstream, but
// a stringstream gets the job done too.)
//
// Good luck, and write beautiful code!
int main(int argc, char **argv) {
  if (argc < 2) {
    Usage(argv[0]);
  }

  // STEP 1:
  // Implement filesearchshell!
  // Probably want to write some helper methods ...

  std::list<std::string> index_list;
  for (int i = 1; i < argc; i++) {
    index_list.push_back(argv[i]);
  }
  hw3::QueryProcessor qp(index_list);  // validate

  while (1) {
    std::cout << "Enter query:" << endl;

    std::string query;  // string frees automatically when out of scope
    std::getline(std::cin, query);  // dynamically allocates mem
                                    // regardless of length
    if (std::cin.eof()) {
      break;
    } else if (!std::cin.good()) {
      cerr << "Reading input failed" << endl;
      continue;
    }

    std::vector<std::string> query_words;
    SplitQuery(query, &query_words);

    std::vector<hw3::QueryProcessor::QueryResult> results
      = qp.ProcessQuery(query_words);
    PrintResults(results);
  }

  return EXIT_SUCCESS;
}

static void Usage(char *prog_name) {
  cerr << "Usage: " << prog_name << " [index files+]" << endl;
  exit(EXIT_FAILURE);
}

static void SplitQuery(const std::string& query,
                       std::vector<std::string>* query_words) {
  std::stringstream ss(query);
  std::string word;
  while (ss >> word) {
    ToLowerWord(&word);
    query_words->push_back(word);
  }
}

static void ToLowerWord(std::string* word) {
  for (int i = 0; (*(word))[i]; i++) {
    (*(word))[i] = tolower((*(word))[i]);
  }
}

static void PrintResults(const std::vector<hw3::QueryProcessor::QueryResult>&
                         results) {
  if (results.empty()) {
    std::cout << "  [no results]" << endl;
  } else {
    for (const auto& result : results) {
      std::cout << "  " << result.document_name
                << " (" << result.rank << ")" << endl;
    }
  }
}
