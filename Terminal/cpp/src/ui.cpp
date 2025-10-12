#include "ui.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

using std::string;
namespace fs = std::filesystem;

static string trim(string s){
    auto l = std::find_if(s.begin(), s.end(), [](unsigned char c){return !std::isspace(c);});
    auto r = std::find_if(s.rbegin(), s.rend(), [](unsigned char c){return !std::isspace(c);}).base();
    if (l >= r) return "";
    return string(l, r);
}

std::vector<City> load_cities(const fs::path& dataDir){
    std::vector<City> out;
    fs::path csv = dataDir / "cities.csv";
    std::ifstream in(csv);
    if (!in) return out;
    string line; bool first=true;
    while (std::getline(in, line)){
        if (first){ first=false; // skip header if present when it contains letters
            string t = line; for(char &c: t) c = (char)std::tolower((unsigned char)c);
            if (t.find("name")!=string::npos && t.find("lat")!=string::npos) continue;
        }
        if (line.empty()) continue;
        std::stringstream ss(line);
        string name,country,lat,lon,tz;
        if (!std::getline(ss, name, ',')) continue;
        std::getline(ss, country, ',');
        std::getline(ss, lat, ',');
        std::getline(ss, lon, ',');
        std::getline(ss, tz, ',');
        City c{};
        c.name=trim(name); c.country=trim(country);
        try{
            c.lat = std::stod(trim(lat));
            c.lon = std::stod(trim(lon));
        }catch(...){ continue; }
        c.tz = trim(tz);
        out.push_back(c);
    }
    return out;
}

// Basic interactive selection: filter by substring, show top 10, choose by number
std::optional<City> select_city_interactive(const std::vector<City>& cities){
    if (cities.empty()) return std::nullopt;
    std::vector<size_t> idx(cities.size());
    for(size_t i=0;i<idx.size();++i) idx[i]=i;

    string query;
    while (true){
        // filter
        std::vector<size_t> filtered;
        string ql = query; for(char &c: ql) c = (char)std::tolower((unsigned char)c);
        for(size_t i: idx){
            string s = cities[i].name + ", " + cities[i].country;
            string sl = s; for(char &c: sl) c = (char)std::tolower((unsigned char)c);
            if (ql.empty() || sl.find(ql)!=string::npos) filtered.push_back(i);
        }
        std::cout << "\nSelect your city (type to search, enter number, or q to cancel)\n";
        std::cout << "Query: " << query << "\n";
        size_t shown = std::min<size_t>(10, filtered.size());
        for(size_t k=0;k<shown;++k){
            const auto &c = cities[filtered[k]];
            std::cout << "  [" << (k+1) << "] " << c.name << ", " << c.country
                      << "  (" << c.lat << ", " << c.lon << ")  tz: " << c.tz << "\n";
        }
        if (filtered.size()>shown) std::cout << "  ... (" << (filtered.size()-shown) << " more)" << "\n";
        std::cout << "> ";
        std::string input; std::getline(std::cin, input);
        if (!std::cin) return std::nullopt;
        input = trim(input);
        if (input=="q" || input=="quit" || input=="exit") return std::nullopt;
        if (input.empty()) { continue; }
        bool isnum = !input.empty() && std::all_of(input.begin(), input.end(), [](unsigned char c){return std::isdigit(c);});
        if (isnum){
            int choice = std::stoi(input);
            if (choice>=1 && (size_t)choice<=shown){
                return cities[filtered[(size_t)choice-1]];
            } else {
                std::cout << "Invalid number.\n";
            }
        } else {
            // Update query
            query = input;
        }
    }
}
