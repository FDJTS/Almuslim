#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct City {
    std::string name;
    std::string country;
    double lat = 0.0;
    double lon = 0.0;
    // Either numeric offset like "+3" or with minutes "+03:30" or a short label like "UTC"
    std::string tz;
};

// Load cities from data/cities.csv under the given data directory
std::vector<City> load_cities(const std::filesystem::path& dataDir);

// Simple interactive selector using standard input/output (works in basic terminals)
// Returns std::nullopt if user cancels.
std::optional<City> select_city_interactive(const std::vector<City>& cities);
