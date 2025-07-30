# Mini Google
A full-stack search engine, implementing everything from data structures to a browser-accessible web interface in C and C++. The final product functions like a miniature version of Google: search input on a browser, your query is served dynamically from disk, and results are ranked and returned in milliseconds.

**Search engine in web browser:**
<img width="1881" height="931" alt="image" src="https://github.com/user-attachments/assets/b4d0dda8-9fa7-41f2-b685-be712515e10c" />


**View webpages:**
<img width="1881" height="942" alt="image" src="https://github.com/user-attachments/assets/9b9e5220-6eb8-443d-8d9e-77f18a0091f0" />

## 👩🏽‍🚀 Demo
Try out Mini Google locally!
```
cd web-server
make
./http333d 5555 ../projdocs unit_test_indices/*
```
Then, you can:
- Search in your browser by visiting http://localhost:5555
- View webpages available under `/static/`, e.g. http://localhost:5555/static/bikeapalooza_2011/index.html.

(For graceful shutdown, append `/quitquitquit` after `...5555/`.)

## 🤸🏽‍♀️ Skills
- **Programming:** C, C++, GoogleTest, Valgrind
- **Data structures:** doubly-linked list, chained hash table, and iterators from scratch
- **Backend systems:** file I/O (standard library & POSIX), directory crawling, inverted index construction
- **Data storage:** byte-level offset management, endian handling
- **Networking:** TCP sockets, HTTP/1.1 protocol
- **Concurrency:** thread pools, mutexes, safe connection handling
- **Security:** cross-site scripting (XSS) escaping, path sanitization, memory leak prevention via Valgrind
- **Frontend integration:** dynamic search through browser UI, static file serving

## 😶‍🌫️ What I Learned
- How to leverage low-level systems for safe memory practices, data integrity, and concurrency
- How real-world tools like search engines use indexing, ranking, and disk I/O optimization
- Why safe memory practices, threading, and protocol parsing are critical in scalable web systems
- How to bridge the gap between systems programming and user interfaces

## 🌳 Directory Overview
```
.
├── data-structures/    # (C) Linked list & hash table implementations
├── file-index-search/  # (C) In-memory file system search engine
├── disk-index/         # (C++) Disk-based indexer & query processor
├── web-server/         # (C++) Multithreaded web server & search frontend
|   └── test_tree/      # Example files and folder structure for crawling & testing
└── projdocs/           # Static website content for search UI
```
### Data structures
This module implements robust, reusable system-level data structures in C: **LinkedList** and **HashTable** using chaining. Both support flexible payloads, aong with iterators and destructors. They are the foundation for the in-memory index.
### In-memory indexing engine
Crawls text files, builds an inverted index, and enables querying from the command line. Emphasizes real-time data management.
- **FileParser** extracts words and positional metadata from documents, mapping word → word's offsets in the document
- **File crawling/indexing**
  - **CrawlFileTree** crawls a directory
  - **DocTable** maintains mappings between file paths ↔ numeric doc IDs
  - **MemIndex** is an inverted index, which maps words → doc IDs → list of offsets in the document
- **searchshell** provides a basic CLI, which receives a user's queries and returns a list of documents ranked based on query term frequency
### Persistent disk-based indexing
Transitions the in-memory structures to disk-backed storage.
- **WriteIndex** serializes DocTable and MemIndex using chained hash tables
- **FileIndexReader** extracts information from an index file and manufactures DocTableReader (which uses an IndexTableReader) and IndexTableReader accessors
  - **DocTableReader** reads the doc ID → doc name table from an index file
  - **DocIDTableReader** reads the doc ID → offsets table from an index file
  - **IndexTableReader** reconstructs data from disk, supporting byte offset parsing, memory mapping, and endianness handling
  - **HashTableReader** is a base class for generic hash table lookup functionality
- **QueryProcessor** loads disk indices and ranks multi-term queries efficiently
### Multithreaded HTTP search server
Implements a web search backend with frontend integration,
- **ServerSocket** creates a server-side listening socket and accepts a client connection represented by **HTTPConnection**
- **HTTPServer:**
  - Thread pool for concurrent requests
  - URL parsing and HTTP response generation
  - Serves dynamic search query results via `search?terms=...`
  - Routes static HTML/CSS/JS files from `projdocs/`
- **http333d** is the web server itself

and fixes security vulnerabilities with error messages.
- **HTML XSS** escaped
- **Directory traversal attacks** disallowed by normalizing and verifying that the client's path names are documents in the directory

## Credits
This project was originally developed as part of UW's CSE 333: Systems Programmming taught by Prof. Hal Perkins. The core design belongs to the course staff. This implementation and documentation reflect my own independent work and enhancements.
