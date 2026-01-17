#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <thread>
#include <algorithm>
#include <future>

using namespace std;

class CsvReader {
public:
    string fileBuffer;
    unordered_map<string_view, int> headerMap;

    // We store data as a vector of blocks to avoid copying during merge
    vector<vector<vector<string_view>>> threadedRows; 
    
    // Flattened view for easy access
    vector<vector<string_view>*> flatIndex; 

    bool load(const string& path) {
        // 1. Read File 
        ifstream file(path, ios::binary | ios::ate);
        if (!file) return false;
        
        size_t fileSize = file.tellg();
        fileBuffer.resize(fileSize);
        file.seekg(0);
        file.read(&fileBuffer[0], fileSize);
        
        // 2. Parse Header (1st line)
        size_t headerEnd = fileBuffer.find('\n');
        if (headerEnd == string::npos) return false; // Empty file or 1 line
        
        parseHeader(string_view(fileBuffer.data(), headerEnd));
        
        // 3. Setup for parallel parsing
        // Starting after the header
        size_t startPos = headerEnd + 1;
        size_t dataSize = fileSize - startPos;
        
        unsigned int numThreads = thread::hardware_concurrency(); 
        if (numThreads == 0) numThreads = 2; // Fallback
        
        //Multiple threads for parsing the data
        cout << "Using " << numThreads << " threads for parsing..." << endl;

        vector<future<vector<vector<string_view>>>> futures;
        size_t chunkSize = dataSize / numThreads;

        size_t currentStart = startPos;

        // 4. Launch Threads
        for (unsigned int i = 0; i < numThreads; ++i) {
            size_t currentEnd;

            if (i == numThreads - 1) {
                // Last thread takes everything remaining
                currentEnd = fileSize;
            } else {
                // Calculate ideal end, then scan forward to find the next newline
                currentEnd = currentStart + chunkSize;
                if (currentEnd >= fileSize) currentEnd = fileSize;
                else {
                    // Align to nearest newline
                    size_t nextNewLine = fileBuffer.find('\n', currentEnd);
                    if (nextNewLine != string::npos) {
                        currentEnd = nextNewLine + 1; // Include the newline
                    } else {
                        currentEnd = fileSize;
                    }
                }
            }

            // Launch Async Task
            futures.push_back(async(launch::async, &CsvReader::parseChunk, this, currentStart, currentEnd));
            
            currentStart = currentEnd;
            if (currentStart >= fileSize) break;
        }

        // 5. Gather Results
        threadedRows.clear();
        flatIndex.clear();
        
        size_t totalRows = 0;
        for (auto& f : futures) {
            threadedRows.push_back(f.get()); // Moves result in
        }

        // 6. Indexing (Fast pointer papping)
        // Instead of copying data to a single vector (slow), we create an index of pointers
        for (auto& block : threadedRows) {
            totalRows += block.size();
            for (auto& row : block) {
                flatIndex.push_back(&row);
            }
        }
        
        return true;
    }

    string_view getValue(size_t rowIndex, const string& colName) {
        if (headerMap.find(colName) == headerMap.end()) return "";
        int colIndex = headerMap[colName];
        
        if (rowIndex < flatIndex.size()) {
            vector<string_view>* rowPtr = flatIndex[rowIndex];
            if (colIndex < rowPtr->size()) {
                return (*rowPtr)[colIndex];
            }
        }
        return "";
    }

    size_t getRowCount() const { return flatIndex.size(); }

private:
    // The worker function that runs on each thread
    vector<vector<string_view>> parseChunk(size_t start, size_t end) {
        vector<vector<string_view>> chunkRows;
        // Optimization: Estimate rows to reserve memory (e.g., 100 bytes per row)
        chunkRows.reserve((end - start) / 100);

        const char* buf = fileBuffer.data();
        size_t cursor = start;
        
        while (cursor < end) {
            vector<string_view> row;
            row.reserve(30); // Pre-allocate typical column count
            
            size_t fieldStart = cursor;
            bool insideQuotes = false;
            bool rowComplete = false;

            // Inner loop: Scan characters
            while (cursor < end) {
                char c = buf[cursor];
                if (c == '"') {
                    insideQuotes = !insideQuotes;
                } else if (c == ',' && !insideQuotes) {
                    row.push_back(createView(fieldStart, cursor));
                    fieldStart = cursor + 1;
                } else if ((c == '\n' || c == '\r') && !insideQuotes) {
                    row.push_back(createView(fieldStart, cursor));
                    
                    if (c == '\r' && cursor + 1 < end && buf[cursor + 1] == '\n') {
                        cursor++;
                    }
                    cursor++;
                    rowComplete = true;
                    break; 
                }
                cursor++;
            }
            
            // Handle edge case: End of chunk without newline
            if (!rowComplete && fieldStart < cursor) {
                row.push_back(createView(fieldStart, cursor));
            }

            if (!row.empty()) {
                chunkRows.push_back(std::move(row));
            }
        }
        return chunkRows;
    }

    string_view createView(size_t start, size_t end) {
        if (start >= end) return "";
        const char* data = fileBuffer.data();
        if (end - start >= 2 && data[start] == '"' && data[end - 1] == '"') {
            return string_view(data + start + 1, end - start - 2);
        }
        return string_view(data + start, end - start);
    }

    void parseHeader(string_view line) {
        size_t start = 0;
        size_t end = 0;
        int index = 0;
        
        // Simple header split (assuming headers don't have commas for simplicity)
        // You can reuse the robust parser here if needed
        while ((end = line.find(',', start)) != string_view::npos) {
            headerMap[line.substr(start, end - start)] = index++;
            start = end + 1;
        }
        headerMap[line.substr(start)] = index;
    }
};
