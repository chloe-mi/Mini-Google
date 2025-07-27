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

#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>  // for stat(): gets file info, used in
                       // S_ISDIR(), S_ISREG(): reg file
#include <unistd.h>
#include <signal.h>
#include <cstdlib>  // for stat, access()
#include <cstdio>
#include <iostream>
#include <list>

#include "./ServerSocket.h"
#include "./HttpServer.h"

using std::cerr;
using std::cout;
using std::endl;
using std::list;
using std::string;

// Print out program usage, and exit() with EXIT_FAILURE.
static void Usage(char *prog_name);

// Parse command-line arguments to get port, path, and indices to use
// for your http333d server.
//
// Params:
// - argc: number of argumnets
// - argv: array of arguments
// - port: output parameter returning the port number to listen on
// - path: output parameter returning the directory with our static files
// - indices: output parameter returning the list of index file names
//
// Calls Usage() on failure. Possible errors include:
// - path is not a readable directory
// - index file names are readable
static void GetPortAndPath(int argc,
                           char **argv,
                           uint16_t *const port,
                           string *const path,
                           list<string> *const indices);

// Validates and sets port number.
// Args:
// - port_str: value to validate and set port to.
// - port: (output param) will hold port number, if valid.
// Returns false if port_str is invalid, true otherwise.
static bool GetPort(const char* port_str, uint16_t *const port);

// Validates and sets path.
// Args:
// - dir: value to validate and set path to.
// - path: (output param) will hold dir, if valid.
// Returns false if dir is invalid
// (doesn't exist, isn't a directory, or isn't readable), true otherwise.
static bool GetPath(char* dir, string* const path);

// Validates and sets indices.
// Args:
// - start_index: index of first file name in argv
// - argc: argv size
// - argv: CLI args; char* elements starting at start_index should be
//         index file names
// - indices: (output param) will hold the file names, if they're valid.
// Returns false if a file name is invalid
// (doesn't exist, isn't a regular file, or isn't readable), true otherwise.
static bool GetIndices(int start_index, int argc, char** argv,
                       list<string>* const indices);

int main(int argc, char **argv) {
  // Print out welcome message.
  cout << "Welcome to http333d, the UW CSE333 web server!" << endl;
  cout << "  Copyright 2012 Steven Gribble" << endl;
  cout << "  http://www.cs.washington.edu/homes/gribble" << endl;
  cout << endl;
  cout << "initializing:" << endl;
  cout << "  parsing port number and static files directory..." << endl;

  // Ignore the SIGPIPE signal, otherwise we'll crash out if a client
  // disconnects unexpectedly.
  signal(SIGPIPE, SIG_IGN);

  // Get the port number and list of index files.
  uint16_t port_num;
  string static_dir;
  list<string> indices;
  GetPortAndPath(argc, argv, &port_num, &static_dir, &indices);
  cout << "    port: " << port_num << endl;
  cout << "    static file path: " << static_dir << endl;

  // Run the server.
  hw4::HttpServer hs(port_num, static_dir, indices);
  if (!hs.Run()) {
    cerr << "  server failed to run!?" << endl;
  }

  cout << "server completed!  Exiting." << endl;
  return EXIT_SUCCESS;
}


static void Usage(char *prog_name) {
  cerr << "Usage: " << prog_name << " port staticfiles_directory indices+";
  cerr << endl;
  exit(EXIT_FAILURE);
}

static void GetPortAndPath(int argc,
                           char **argv,
                           uint16_t *const port,
                           string *const path,
                           list<string> *const indices) {
  // Here are some considerations when implementing this function:
  // - There is a reasonable number of command line arguments
  // - The port number is reasonable
  // - The path (i.e., argv[2]) is a readable directory
  // - You have at least 1 index, and all indices are readable files

  // STEP 1:
  if (argc < 4) Usage(argv[0]);

  if (!GetPort(argv[1], port)) Usage(argv[0]);
  if (!GetPath(argv[2], path)) Usage(argv[0]);
  if (!GetIndices(3, argc, argv, indices)) Usage(argv[0]);
}

static bool GetPort(const char* port_str, uint16_t *const port) {
  // resource: how to validate port num: https://ihateregex.io/expr/port/
  char* str_end;  // after num, should be no more string
  #define BASE 10
  uint16_t port_num = static_cast<uint16_t>(strtol(port_str, &str_end, BASE));
  #undef BASE
  #define PORT_LOWER_BOUND 1024
  #define PORT_UPPER_BOUND 65535
  if (*str_end != '\0' || port_num < PORT_LOWER_BOUND
                       || port_num > PORT_UPPER_BOUND) {
    #undef PORT_LOWER_BOUND
    #undef PORT_UPPER_BOUND
    return false;
  }
  #undef PORT_LOWER_BOUND
  #undef PORT_UPPER_BOUND
  *port = port_num;
  return true;
}

static bool GetPath(char* dir, string* const path) {
  struct stat stat_struct;
  if (stat(dir, &stat_struct) == -1     // dir exists?
      || !S_ISDIR(stat_struct.st_mode)  // is a dir?
      || access(dir, R_OK) == -1) {     // is readable?
    return false;
  }
  *path = dir;
  return true;
}

static bool GetIndices(int start_index, int argc, char** argv,
                       list<string>* const indices) {
  // remaining args should be indices
  struct stat stat_struct;
  for (int i = start_index; i < argc; i++) {
    const char* index_file = argv[i];
    if (stat(index_file, &stat_struct) == -1
        || !S_ISREG(stat_struct.st_mode)
        || access(index_file, R_OK) == -1) {
      return false;
    }
    indices->push_back(index_file);
  }
  return true;
}
