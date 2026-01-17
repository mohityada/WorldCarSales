#include <string>
#include <iostream>
#include <map>
#include <string_view>
#include <algorithm>

using namespace std;


class SalesService {
    
    public:
        long long countSales(unordered_map<string_view, int> headerMap, vector<vector<string_view>*> flatIndex, string targetBrand, string targetCountry, 
            string targetYear) {
        // 1. Validate Columns exist
        if (headerMap.find("manufacturer") == headerMap.end() || 
            headerMap.find("country") == headerMap.end() || 
            headerMap.find("sale_date") == headerMap.end()) {
            cerr << "Error: Required columns missing in CSV." << endl;
            return -1;
        }

        // 2. Get Indices once (Fast lookup)
        int brandIdx = headerMap["manufacturer"];
        int countryIdx = headerMap["country"];
        int dateIdx = headerMap["sale_date"];
        
        long long count = 0;

        // 3. Iterate over the existing index (Zero copying)
        for (const auto* rowPtr : flatIndex) {
            // Dereference the pointer to get the actual row vector
            const auto& row = *rowPtr;

            // Safety check: ensure row has enough columns
            if (row.size() <= max({brandIdx, countryIdx, dateIdx})) continue;

            // Check Brand (Audi)
            if (row[brandIdx] == targetBrand) {
                
                // Check Country (China)
                if (row[countryIdx] == targetCountry) {
                    
                    // Check Year (Starts with targetYear, e.g., "2025")
                    string_view dateStr = row[dateIdx];
                    if (dateStr.size() >= 4 && dateStr.substr(0, 4) == targetYear) {
                        count++;
                    }
                }
            }
        }
        return count;
    }
};