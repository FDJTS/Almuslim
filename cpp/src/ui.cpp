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
            // Strip UTF-8 BOM on the first line if present
            if (first && line.size()>=3 && (unsigned char)line[0]==0xEF && (unsigned char)line[1]==0xBB && (unsigned char)line[2]==0xBF) {
                line.erase(0,3);
            }
            if (first){
                first=false; // skip header if present
                string t = line; for(char &c: t) c = (char)std::tolower((unsigned char)c);
                if (t.find("name")!=string::npos && t.find("lat")!=string::npos) continue;
            }
            string trimmed = trim(line);
            if (trimmed.empty()) continue;
            if (!trimmed.empty() && trimmed[0]=='#') continue; // comment line
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

// Normalize strings for matching: lowercase, remove punctuation, apply synonyms
static std::string normalize_str(std::string s){
    // lowercase
    for(char &c: s) c = (char)std::tolower((unsigned char)c);
    // remove accents (basic): map a few common ones
    auto rep_all = [&](const std::string &from, const std::string &to){ size_t pos=0; while((pos = s.find(from, pos)) != std::string::npos){ s.replace(pos, from.size(), to); pos += to.size(); } };
    rep_all("á","a"); rep_all("à","a"); rep_all("â","a"); rep_all("ã","a"); rep_all("ä","a");
    rep_all("ç","c"); rep_all("é","e"); rep_all("è","e"); rep_all("ê","e"); rep_all("ë","e");
    rep_all("í","i"); rep_all("ì","i"); rep_all("î","i"); rep_all("ï","i"); rep_all("ñ","n");
    rep_all("ó","o"); rep_all("ò","o"); rep_all("ô","o"); rep_all("õ","o"); rep_all("ö","o");
    rep_all("ú","u"); rep_all("ù","u"); rep_all("û","u"); rep_all("ü","u"); rep_all("ý","y");
    // remove apostrophes and non-alnum (keep space, comma)
    std::string out; out.reserve(s.size());
    for(char c: s){ if (std::isalnum((unsigned char)c) || c==' ' || c==',') out.push_back(c); }
    s.swap(out);
    // collapse spaces
    std::string out2; out2.reserve(s.size()); bool prevSpace=false; for(char c: s){ if (std::isspace((unsigned char)c)){ if(!prevSpace){ out2.push_back(' '); prevSpace=true; } } else { out2.push_back(c); prevSpace=false; } }
    s.swap(out2);
    // synonyms/transliterations
    auto replace_word = [&](const std::string &from, const std::string &to){
        size_t pos = 0; while(true){ pos = s.find(from, pos); if (pos==std::string::npos) break; bool okL = (pos==0 || s[pos-1]==' '|| s[pos-1]==','); size_t end = pos+from.size(); bool okR = (end>=s.size() || s[end]==' ' || s[end]==','); if (okL && okR){ s.replace(pos, from.size(), to); pos += to.size(); } else { pos = end; } }
    };
    // common transliteration variants
    replace_word("mecca", "makkah");
    replace_word("medina", "madinah");
    replace_word("buraydah", "buraidah");
    replace_word("jazan", "jizan");
    return s;
}

// Levenshtein distance for fuzzy matching
static int lev_distance(const std::string &a, const std::string &b){
    size_t n=a.size(), m=b.size();
    if (n==0) return (int)m; if (m==0) return (int)n;
    std::vector<int> prev(m+1), curr(m+1);
    for(size_t j=0;j<=m;++j) prev[j] = (int)j;
    for(size_t i=1;i<=n;++i){
        curr[0] = (int)i;
        for(size_t j=1;j<=m;++j){
            int cost = (a[i-1]==b[j-1])?0:1;
            curr[j] = std::min({ prev[j] + 1, curr[j-1] + 1, prev[j-1] + cost });
        }
        std::swap(prev, curr);
    }
    return prev[m];
}

// Compute a composite score: substring proximity + fuzzy match (lower is better distance)
static int score_match(const std::string& hay_raw, const std::string& needle_raw){
    std::string hay = normalize_str(hay_raw);
    std::string needle = normalize_str(needle_raw);
    if (needle.empty()) return -1000000;
    int score = -1000000;
    auto pos = hay.find(needle);
    if (pos != std::string::npos){
        int proximity = (int)std::max<size_t>(0, 200 - pos);
        int length_bias = (int)std::max<int>(0, 100 - std::abs((int)hay.size() - (int)needle.size()));
        score = std::max(score, proximity + length_bias);
    }
    // fuzzy against city token only as well as full "name, country"
    int d1 = lev_distance(hay, needle);
    int fuzzy = 300 - d1 * 20 - (int)std::abs((int)hay.size() - (int)needle.size());
    score = std::max(score, fuzzy);
    return score;
}

std::optional<City> find_best_city_match(const std::vector<City>& cities, const std::string& query){
    if (cities.empty()) return std::nullopt;
    if (query.empty()) return std::nullopt;
    std::string q = query;
    // First try exact normalized match on city or "city, country"
    std::string qn = normalize_str(q);
    for (const auto &c : cities){
        std::string n1 = normalize_str(c.name);
        std::string n2 = normalize_str(c.name + ", " + c.country);
        if (qn == n1 || qn == n2) return c;
    }
    int bestScore = -1000000; size_t bestIdx = 0;
    for(size_t i=0;i<cities.size();++i){
        std::string s = cities[i].name + ", " + cities[i].country;
        int sc = score_match(s, q);
        if (sc > bestScore){ bestScore = sc; bestIdx = i; }
    }
    if (bestScore < 0) return std::nullopt;
    return cities[bestIdx];
}

std::optional<City> prompt_city_free_text(const std::vector<City>& cities){
    if (cities.empty()) return std::nullopt;
    std::cout << "\nType your city (e.g., Riyadh or Riyadh, Saudi Arabia). Type 'q' to cancel.\n> ";
    std::string input; if (!std::getline(std::cin, input)) return std::nullopt;
    std::string t = trim(input);
    if (t == "" || t == "q" || t == "quit" || t == "exit") return std::nullopt;
    auto m = find_best_city_match(cities, t);
    if (!m){
        std::cout << "Could not find a close match for '" << t << "'.\n";
        return std::nullopt;
    }
    const City &c = *m;
    std::cout << "Using: " << c.name << ", " << c.country
              << "  (" << c.lat << ", " << c.lon << ") tz: " << c.tz << "\n";
    return m;
}
