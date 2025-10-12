#pragma once
#include <optional>
#include <string>
#include <filesystem>
#include <ctime>
#include <vector>

namespace hijri {

struct HijriDate {
    int year{};   // AH
    int month{};  // 1..12
    int day{};    // 1..30
};

// Load Umm al-Qura month starts from CSV file at dataDir/hijri/umm_al_qura_month_starts.csv
// CSV format (with header): hijri_year,hijri_month,gregorian_yyyy,gregorian_mm,gregorian_dd
bool load_umm_al_qura(const std::filesystem::path& dataDir);

// Convert a local calendar date (struct tm) to Hijri using the loaded table.
// Returns std::nullopt if table not loaded or date out of range.
std::optional<HijriDate> hijri_for_date(const std::tm& localDate);

// Utility to format Hijri month names (English/Arabic)
const char* month_name_en(int m);
const char* month_name_ar(int m);

} // namespace hijri
