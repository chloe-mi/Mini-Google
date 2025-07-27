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

#include <stdint.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <map>
#include <string>
#include <vector>
#include <iostream>  // cerr

#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpConnection.h"

using std::map;
using std::string;
using std::vector;

namespace hw4 {

static const char *kHeaderEnd = "\r\n\r\n";
static const int kHeaderEndLen = 4;
static const int kBufSize = 1024;

// Reads contents from the file specified by fd into buffer.
// Stops when buffer contains a header ending \r\n\r\n.
// Args:
// - fd: this connection's file descriptor associated with the client.
// - buffer: (output param) this connection's buffer storing data
//           read from the client.
// Returns the position of the ending in buffer,
// or returns -1 if read failure or EOF and no bytes read.
static int ReadFromFileToBuffer(const int& fd, string* const buffer);

// Extracts header and value from line. Adds to req (output param).
// line should be of form [headername]: [headerval]\r\n
// Returns false if line is malformed, true if successful.
static bool AddHeaderNameAndVal(const string& line, HttpRequest* const req);

bool HttpConnection::GetNextRequest(HttpRequest *const request) {
  // Use WrappedRead from HttpUtils.cc to read bytes from the files into
  // private buffer_ variable. Keep reading until:
  // 1. The connection drops
  // 2. You see a "\r\n\r\n" indicating the end of the request header.
  //
  // Hint: Try and read in a large amount of bytes each time you call
  // WrappedRead.
  //
  // After reading complete request header, use ParseRequest() to parse into
  // an HttpRequest and save to the output parameter request.
  //
  // Important note: Clients may send back-to-back requests on the same socket.
  // This means WrappedRead may also end up reading more than one request.
  // Make sure to save anything you read after "\r\n\r\n" in buffer_ for the
  // next time the caller invokes GetNextRequest()!

  // STEP 1:

  int header_end_pos = ReadFromFileToBuffer(fd_, &buffer_);
  if (header_end_pos == -1) return false;

  // separate curr and next requests
  string curr_request = buffer_.substr(0, header_end_pos
                                          + kHeaderEndLen);
  if (static_cast<size_t>(header_end_pos + kHeaderEndLen) < buffer_.size()) {
    buffer_ = buffer_.substr(header_end_pos + kHeaderEndLen);  // buffer=next
  } else {
    buffer_.clear();
  }
  *request = ParseRequest(curr_request);
  if ((request->uri()).compare("uri for poorly formed request") == 0
      || (request->uri()).compare("no lines in request") == 0) {
        return false;  // poorly formed request
  } else {
    return true;
  }
}

static int ReadFromFileToBuffer(const int& fd, string* const buffer) {
  int header_end_pos = static_cast<int>(buffer->find(kHeaderEnd));
                       // from size_t
  while (static_cast<size_t>(header_end_pos) == string::npos) {
    char buf[kBufSize];
    int bytes_read = WrappedRead(fd, reinterpret_cast<unsigned char*>(buf),
                                 kBufSize);

    if (bytes_read <= 0) {  // 0: EOF and no bytes read
      if (bytes_read == -1) {
        std::cerr << "wrapped read failed to read from fd" << std::endl;
      }
      return -1;
    }

    *buffer += string(buf, bytes_read);  // instead of unsigned

    header_end_pos = static_cast<int>(buffer->find(kHeaderEnd));
  }
  return header_end_pos;
}

HttpRequest HttpConnection::ParseRequest(const string &request) const {
  HttpRequest req("/");  // by default, get "/".

  // Plan for STEP 2:
  // 1. Split the request into different lines (split on "\r\n").
  // 2. Extract the URI from the first line and store it in req.URI.
  // 3. For the rest of the lines in the request, track the header name and
  //    value and store them in req.headers_ (e.g. HttpRequest::AddHeader).
  //
  // Hint: Take a look at HttpRequest.h for details about the HTTP header
  // format that you need to parse.
  //
  // You'll probably want to look up boost functions for:
  // - Splitting a string into lines on a "\r\n" delimiter
  // - Trimming whitespace from the end of a string
  // - Converting a string to lowercase.
  //
  // Note: If a header is malformed, skip that line.

  // STEP 2:

  // 1
  vector<string> lines;
  boost::split(lines, request, boost::is_any_of("\r\n"),
               boost::token_compress_on);
  if (lines.empty()) {
    std::cerr << "no lines in request" << std::endl;
    req.set_uri("no lines in request");
    return req;
  }

  // 2
  vector<string> get_tokens;
  boost::split(get_tokens, lines[0], boost::is_any_of(" "),
               boost::token_compress_on);
  #define GOOD_NUM_TOKENS 3
  if (get_tokens[0].compare("GET") != 0
      || get_tokens.size() != GOOD_NUM_TOKENS) {
    #undef GOOD_NUM_TOKENS
    std::cerr << "poorly formed request" << std::endl;
    req.set_uri("uri for poorly formed request");
    return req;
  }
  #undef GOOD_NUM_TOKENS
  req.set_uri(get_tokens[1]);

  // 3
  for (size_t i = 1; i < lines.size(); i++) {
    AddHeaderNameAndVal(lines[i], &req);
  }
  return req;
}

static bool AddHeaderNameAndVal(const string& line, HttpRequest* const req) {
  size_t colon_pos = line.find(":");
  if (colon_pos == string::npos) return false;  // malformed header

  string name = line.substr(0, colon_pos);
  boost::trim(name);
  if (colon_pos > line.size()) return false;
  string value = line.substr(colon_pos + 1);
  boost::trim(value);

  req->AddHeader(name, value);
  return true;
}

bool HttpConnection::WriteResponse(const HttpResponse &response) const {
  // We use a reinterpret_cast<> to cast between unrelated pointer types, and
  // a static_cast<> to perform a conversion from an unsigned type to its
  // corresponding signed one.
  string str = response.GenerateResponseString();
  int res = WrappedWrite(fd_,
                         reinterpret_cast<const unsigned char*>(str.c_str()),
                         str.length());

  if (res != static_cast<int>(str.length()))
    return false;
  return true;
}

}  // namespace hw4
