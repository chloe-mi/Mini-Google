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

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <sstream>

#include "./FileReader.h"
#include "./HttpConnection.h"
#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpServer.h"
#include "./libhw3/QueryProcessor.h"

using std::cerr;
using std::cout;
using std::endl;
using std::list;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;

namespace hw4 {
///////////////////////////////////////////////////////////////////////////////
// Constants, internal helper functions
///////////////////////////////////////////////////////////////////////////////
// static
const int HttpServer::kNumThreads = 8;

static const char *kThreegleStr =
  "<html><head><title>chloogl&euml;</title></head>\n"
  "<body>\n"
  "<center style=\"font-size:500%;\">\n"
  "<span style=\"color:blue;\">c</span>"
    "<span style=\"color:red;\">h</span>"
    "<span style=\"color:green;\">l</span>"
    "<span style=\"color:blue;\">o</span>"
    "<span style=\"color:gold;\">o</span>"
    "<span style=\"color:red;\">g</span>"
    "<span style=\"color:blue;\">l</span>"
    "<span style=\"color:green;\">&euml;</span>\n"
  "</center>\n"
  "<p>\n"
  "<div style=\"height:20px;\"></div>\n"
  "<center>\n"
  "<form action=\"/query\" method=\"get\">\n"
  "<input type=\"text\" size=30 name=\"terms\" />\n"
  "<input type=\"submit\" value=\"Search\" />\n"
  "</form>\n"
  "</center><p>\n";

// This is the function that threads are dispatched into
// in order to process new client connections.
static void HttpServer_ThrFn(ThreadPool::Task *t);

// Given a request, produce a response.
static HttpResponse ProcessRequest(const HttpRequest &req,
                                   const string &base_dir,
                                   const list<string> &indices);

// Process a file request.
static HttpResponse ProcessFileRequest(const string &uri,
                                       const string &base_dir);

// Extracts file name out of uri, stores in file_name (output param).
// Uses resulting file_name and base_dir to build path (output param).
static void BuildPath(const string &uri, const string &base_dir,
                      string* const file_name, string* const path);

// Sets ret's content type based on path.
static void SetContentType(const string& path, HttpResponse* ret);

// Process a query request.
static HttpResponse ProcessQueryRequest(const string &uri,
                                        const list<string> &indices,
                                        const string &base_dir);

// Returns vector of query tokens from uri.
static std::vector<string> GetQueryVec(const string &uri);

// Returns html string that displays "<num_results> results found for <query>"
static string NumResultsFor(const int& num_results,
                            const std::vector<string>& query);

// Returns html string that displays an unordered list of hyperlinked results.
// Removes base_dir from doc names if applicable.
static string HtmlResults(const std::vector<hw3::QueryProcessor::QueryResult>&
                                                                      results,
                          const string &base_dir);

// Returns true if 's' starts with 'prefix'.
static bool StringStartsWith(const string &s, const string &prefix);

///////////////////////////////////////////////////////////////////////////////
// HttpServer
///////////////////////////////////////////////////////////////////////////////
bool HttpServer::Run(void) {
  // Create the server listening socket.
  int listen_fd;
  cout << "  creating and binding the listening socket..." << endl;
  if (!socket_.BindAndListen(AF_INET6, &listen_fd)) {
    cerr << endl << "Couldn't bind to the listening socket." << endl;
    return false;
  }

  // Spin, accepting connections and dispatching them.  Use a
  // threadpool to dispatch connections into their own thread.
  tp_.reset(new ThreadPool(kNumThreads));
  cout << "  accepting connections..." << endl << endl;
  while (!IsShuttingDown()) {
    // If the HST is successfully added to the threadpool, it'll (eventually)
    // get run and clean itself up.  But we need to manually delete it if
    // it doesn't get added.
    HttpServerTask *hst = new HttpServerTask(HttpServer_ThrFn, this);
    hst->base_dir = static_file_dir_path_;
    hst->indices = &indices_;

    if (!socket_.Accept(&hst->client_fd,
                        &hst->c_addr,
                        &hst->c_port,
                        &hst->c_dns,
                        &hst->s_addr,
                        &hst->s_dns)) {
      // The accept failed for some reason, so quit out of the server.  This
      // can happen when the `kill` command is used to shut down the server
      // instead of the more graceful /quitquitquit handler.
      delete hst;
      break;
    }

    // The accept succeeded; dispatch it to the workers.
    if (!tp_->Dispatch(hst)) {
      delete hst;
      break;
    }
  }
  return true;
}

void HttpServer::BeginShutdown() {
  Verify333(pthread_mutex_lock(&lock_) == 0);
  shutting_down_ = true;
  tp_->BeginShutdown();
  Verify333(pthread_mutex_unlock(&lock_) == 0);
}

bool HttpServer::IsShuttingDown() {
  bool retval;
  Verify333(pthread_mutex_lock(&lock_) == 0);
  retval = shutting_down_;
  Verify333(pthread_mutex_unlock(&lock_) == 0);
  return retval;
}

///////////////////////////////////////////////////////////////////////////////
// Internal helper functions
///////////////////////////////////////////////////////////////////////////////
static void HttpServer_ThrFn(ThreadPool::Task *t) {
  // Cast back our HttpServerTask structure with all of our new client's
  // information in it.  Since we the ones that created this object, we are
  // guaranteed that this is an instance of a HttpServerTask and, per Google's
  // Style Guide, can use a static_cast<> instead of a dynamic_cast<>.
  //
  // Note that, per the ThreadPool::Task API, it is the job of this function
  // to clean up the dynamically-allocated task object.
  unique_ptr<HttpServerTask> hst(static_cast<HttpServerTask*>(t));
  cout << "  client " << hst->c_dns << ":" << hst->c_port << " "
       << "(IP address " << hst->c_addr << ")" << " connected." << endl;

  // Read in the next request, process it, and write the response.

  // Use the HttpConnection class to read and process the next HTTP request
  // from our current client, then write out our response.  Recall that
  // multiple HTTP requests can be sent on the same TCP connection; we
  // need to keep the connection alive until the client sends a
  // "Connection: close\r\n" header; it is only after we finish processing
  // their request that we can shut down the connection and exit
  // this function.

  // STEP 1:
  HttpRequest rq;  // you should probably initialize this somehow
  HttpConnection conn(hst->client_fd);
  while (!hst->server_->IsShuttingDown()) {
    if (!conn.GetNextRequest(&rq)) {
      continue;
    }

    // If the client requested the server to shut down, do so.
    if (StringStartsWith(rq.uri(), "/quitquitquit")) {
      hst->server_->BeginShutdown();
      break;
    }

    HttpResponse resp = ProcessRequest(rq, hst->base_dir, *(hst->indices));
    if (!conn.WriteResponse(resp)) {
      cerr << "writing response failed since connection had error, "
           << "should be closed" << endl;
      continue;
    }

    if (static_cast<string>(rq.GetHeaderValue("Connection")).compare("close")
        == 0) {
      // nice safe close!
      break;
    }
  }
}

static HttpResponse ProcessRequest(const HttpRequest &req,
                                   const string &base_dir,
                                   const list<string> &indices) {
  // Is the user asking for a static file?
  if (StringStartsWith(req.uri(), "/static/")) {
    return ProcessFileRequest(req.uri(), base_dir);
  }

  // The user must be asking for a query.
  return ProcessQueryRequest(req.uri(), indices, base_dir);
}

static HttpResponse ProcessFileRequest(const string &uri,
                                       const string &base_dir) {
  // The response we'll build up.
  HttpResponse ret;

  // Steps to follow:
  // 1. Use the URLParser class to figure out what file name
  //    the user is asking for. Note that we identify a request
  //    as a file request if the URI starts with '/static/'
  //
  // 2. Use the FileReader class to read the file into memory
  //
  // 3. Copy the file content into the ret.body
  //
  // 4. Depending on the file name suffix, set the response
  //    Content-type header as appropriate, e.g.,:
  //      --> for ".html" or ".htm", set to "text/html"
  //      --> for ".jpeg" or ".jpg", set to "image/jpeg"
  //      --> for ".png", set to "image/png"
  //      etc.
  //    You should support the file types mentioned above,
  //    as well as ".txt", ".js", ".css", ".xml", ".gif",
  //    and any other extensions to get bikeapalooza
  //    to match the solution server.
  //
  // be sure to set the response code, protocol, and message
  // in the HttpResponse as well.

  // STEP 2:
  // 1
  string file_name = "";
  string path = "";
  BuildPath(uri, base_dir, &file_name, &path);

  // set response fields
  ret.set_protocol("HTTP/1.1");  // HTTP to WebSocket

  // 2
  FileReader file_reader(base_dir, file_name);
  string content;
  if (!file_reader.ReadFile(&content)) {
    // If you couldn't find the file, return an HTTP 404 error.
    ret.set_response_code(404);
    ret.set_message("Not Found");
    ret.AppendToBody("<html><body>Couldn't find file \""
                    + EscapeHtml(file_name)
                    + "\"</body></html>\n");
    ret.set_content_type("text/html");
    return ret;

  } else {
    ret.set_response_code(200);
    ret.set_message("OK");
    // 3
    ret.AppendToBody(content);
    // 4
    SetContentType(path, &ret);
    return ret;
  }
}

static void BuildPath(const string &uri, const string &base_dir,
                      string* const file_name, string* const path) {
  // get file name
  URLParser url_parser;
  url_parser.Parse(uri);
  // (remove static since file name isn't actually in static subdir)
  *file_name = url_parser.path().substr(strlen("/static/"));

  // path = base_dir/file_name
  *path = base_dir;
  if (!path->empty() && path->back() != '/') {
    *path += '/';
  }
  *path += *file_name;
}

static void SetContentType(const string& path, HttpResponse* ret) {
  // reference:
  // https://developer.mozilla.org/en-US/docs/Web/HTTP/Guides/MIME_types/
  // Common_types
  if (boost::algorithm::ends_with(path, ".html") ||
      boost::algorithm::ends_with(path, ".htm")) {
    ret->set_content_type("text/html");
  } else if (boost::algorithm::ends_with(path, ".jpg") ||
            boost::algorithm::ends_with(path, ".jpeg")) {
    ret->set_content_type("image/jpeg");
  } else if (boost::algorithm::ends_with(path, ".png")) {
    ret->set_content_type("image/png");
  } else if (boost::algorithm::ends_with(path, ".js")) {
    ret->set_content_type("text/javascript");
  } else if (boost::algorithm::ends_with(path, ".css")) {
    ret->set_content_type("text/css");
  } else if (boost::algorithm::ends_with(path, ".xml")) {
    ret->set_content_type("application/xml");
  } else if (boost::algorithm::ends_with(path, ".gif")) {
    ret->set_content_type("image/gif");
  } else {
    ret->set_content_type("text/plain");  // .txt included
  }
}

static HttpResponse ProcessQueryRequest(const string &uri,
                                        const list<string> &indices,
                                        const string &base_dir) {
  // The response we're building up.
  HttpResponse ret;

  // Your job here is to figure out how to present the user with
  // the same query interface as our solution_binaries/http333d server.
  // A couple of notes:
  //
  // 1. The 333gle logo and search box/button should be present on the site.
  //
  // 2. If the user had previously typed in a search query, you also need
  //    to display the search results.
  //
  // 3. you'll want to use the URLParser to parse the uri and extract
  //    search terms from a typed-in search query.  convert them
  //    to lower case.
  //
  // 4. Initialize and use hw3::QueryProcessor to process queries with the
  //    search indices.
  //
  // 5. With your results, try figuring out how to hyperlink results to file
  //    contents, like in solution_binaries/http333d. (Hint: Look into HTML
  //    tags!)

  // STEP 3:

  ret.set_protocol("HTTP/1.1");
  ret.set_response_code(200);
  ret.set_message("OK");
  ret.set_content_type("html");

  // 1
  // response body: HTML
  // resource: https://developer.mozilla.org/en-US/docs/Web/HTML/Reference/Elements
  stringstream html;
  html << kThreegleStr;  // close body and html

  // 3
  std::vector<string> query = GetQueryVec(uri);
  std::vector<string> escaped_query;
  for (const string& term : query) {
    escaped_query.push_back(EscapeHtml(term));
  }

  // 4
  hw3::QueryProcessor qp(indices);
  std::vector<hw3::QueryProcessor::QueryResult> results;
  if (!query.empty()) results = qp.ProcessQuery(query);

  // 5
  // num results found for query
  int num_results = results.size();
  html << NumResultsFor(num_results, escaped_query);

  if (num_results == 0) {
    html << "</body></html>";
    ret.AppendToBody(html.str());
    return ret;
  }

  // at least 1 result, show links
  html << HtmlResults(results, base_dir);
  ret.AppendToBody(html.str());
  return ret;
}

static std::vector<string> GetQueryVec(const string &uri) {
  // get terms=... from uri
  URLParser url_parser;
  url_parser.Parse(uri);
  map<string, string> args = url_parser.args();  // field=value
  string terms = args["terms"];

  // pull tokens out of terms, push to vec
  std::vector<string> query;
  stringstream terms_it(terms);
  string term;
  while (getline(terms_it, term, ' ')) {
    std::transform(term.begin(), term.end(), term.begin(), tolower);
    query.push_back(term);
  }
  return query;
}

static string NumResultsFor(const int& num_results,
                            const std::vector<string>& query) {
  stringstream local_html;

  // num results found for
  local_html << (num_results >= 1 ? std::to_string(num_results) : "No")
             << " results found for ";

  // query
  // fencepost space, <b>: bold
  if (!query.empty()) local_html << "<b>" << query[0];
  for (int i = 1; i < static_cast<int>(query.size()); i++) {
    local_html << " " << query[i];
  }
  local_html << "</b><br>";  // <br>: line breaks
  return local_html.str();
}

static bool StringStartsWith(const string &s, const string &prefix) {
  return s.substr(0, prefix.size()) == prefix;
}

static string HtmlResults(const std::vector<hw3::QueryProcessor::QueryResult>&
                                                                      results,
                          const string &base_dir) {
  stringstream local_html;
  local_html << "<ul>";
  for (const hw3::QueryProcessor::QueryResult& result : results) {
    string doc_name = result.document_name;
    // <li>: list item, <a>: link
    local_html << "<li><a href=\"";
    if (StringStartsWith(doc_name, base_dir)) {
      // remove base_dir
      doc_name = doc_name.substr(base_dir.length());
      // remove /
      if (!doc_name.empty() && doc_name[0] == '/') {
        #define INDEX_AFTER_SLASH 1
        doc_name = doc_name.substr(INDEX_AFTER_SLASH);
        #undef INDEX_AFTER_SLASH
      }
    } else if (!StringStartsWith(doc_name, "http")) {
      local_html << "/static/";
    }
    string escaped_doc_name = EscapeHtml(doc_name);
    local_html << escaped_doc_name << "\">"
         << escaped_doc_name << "</a>"
         << " [" << result.rank << "]</li>";
  }
  local_html << "</ul>";
  local_html << "</body></html>";
  return local_html.str();
}

}  // namespace hw4
