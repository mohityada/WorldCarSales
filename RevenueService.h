#pragma once

#include "CsvReader.h" 
#include <iostream>
#include <string>
#include <string_view>
#include <algorithm>

using namespace std;

// Structure to hold the result for sorting
struct CountryRevenue {
    string country;
    double revenue;
};

// Comparator for sorting descending
bool compareRevenue(const CountryRevenue& a, const CountryRevenue& b) {
    return a.revenue > b.revenue;
}
class RevenueService {
public:
    double calculateTotalRevenue(CsvReader& reader, string targetBrand, string targetYear) {
        
        // 1. Validate Columns
        if (reader.headerMap.find("manufacturer") == reader.headerMap.end() || 
            reader.headerMap.find("sale_date") == reader.headerMap.end() || 
            reader.headerMap.find("sale_price_usd") == reader.headerMap.end()) {
            cerr << "[RevenueService] Error: Required columns (manufacturer, sale_date, sale_price_usd) missing." << endl;
            return 0.0;
        }

        // 2. Get Column Indices (Fast Lookup)
        int brandIdx = reader.headerMap["manufacturer"];
        int dateIdx  = reader.headerMap["sale_date"];
        int priceIdx = reader.headerMap["sale_price_usd"];

        double totalRevenue = 0.0;
        int targetYearLen = targetYear.length();

        // 3. High-Speed Iteration
        for (const auto* rowPtr : reader.flatIndex) {
            const auto& row = *rowPtr;

            // Safety Check
            if (row.size() <= max({brandIdx, dateIdx, priceIdx})) continue;

            // FILTER 1: Manufacturer
            if (row[brandIdx] == targetBrand) {
                
                // FILTER 2: Year
                string_view dateStr = row[dateIdx];
                if (dateStr.size() >= targetYearLen && 
                    dateStr.substr(0, targetYearLen) == targetYear) {
                    
                    // AGGREGATION: Manual Fast Parse (Replaces from_chars)
                    string_view priceStr = row[priceIdx];
                    double price = 0.0;
                    double divisor = 1.0;
                    bool decimalFound = false;

                    // Simple loop to parse "12345.50" directly from view
                    for (char c : priceStr) {
                        if (c >= '0' && c <= '9') {
                            price = price * 10.0 + (c - '0');
                            if (decimalFound) divisor *= 10.0;
                        } else if (c == '.') {
                            decimalFound = true;
                        }
                    }
                    
                    if (divisor > 1.0 || price > 0.0) {
                        totalRevenue += (price / divisor);
                    }
                }
            }
        }

        return totalRevenue;
    }

    vector<CountryRevenue> getRevenueByCountry(CsvReader& reader, string targetBrand, string targetRegion) {
        
        // 1. Validate Columns
        if (reader.headerMap.count("manufacturer") == 0 || 
            reader.headerMap.count("region") == 0 || 
            reader.headerMap.count("country") == 0 || 
            reader.headerMap.count("sale_price_usd") == 0) {
            cerr << "Error: Missing required columns." << endl;
            return {};
        }

        int brandIdx   = reader.headerMap["manufacturer"];
        int regionIdx  = reader.headerMap["region"];
        int countryIdx = reader.headerMap["country"];
        int priceIdx   = reader.headerMap["sale_price_usd"];

        // 2. Aggregate using a Map (Country -> Total Revenue)
        unordered_map<string, double> revenueMap;

        for (const auto* rowPtr : reader.flatIndex) {
            const auto& row = *rowPtr;
            if (row.size() <= max({brandIdx, regionIdx, countryIdx, priceIdx})) continue;

            // Filter by Brand (BMW) AND Region (Europe)
            if (row[brandIdx] == targetBrand && row[regionIdx] == targetRegion) {
                
                // Parse Price
                string_view priceStr = row[priceIdx];
                double price = 0.0;
                double divisor = 1.0;
                bool decimalFound = false;

                for (char c : priceStr) {
                    if (c >= '0' && c <= '9') {
                        price = price * 10.0 + (c - '0');
                        if (decimalFound) divisor *= 10.0;
                    } else if (c == '.') {
                        decimalFound = true;
                    }
                }
                
                if (divisor > 1.0 || price > 0.0) {
                    // Accumulate Revenue for this Country
                    // Convert string_view to string key for the map
                    string country(row[countryIdx]);
                    revenueMap[country] += (price / divisor);
                }
            }
        }

        // 3. Convert Map to Vector for Sorting
        vector<CountryRevenue> result;
        for (auto const& [country, revenue] : revenueMap) {
            result.push_back({country, revenue});
        }

        // 4. Sort Descending (Highest to Lowest)
        sort(result.begin(), result.end(), compareRevenue);

        return result;
    }
    
};