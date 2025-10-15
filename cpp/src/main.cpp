#include <chrono>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <vector>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

#include "platform.hpp"
#include "ui.hpp"
#include "hijri.hpp"

#if defined(_WIN32)
#include <windows.h>
#include <limits>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
// Enable UTF-8 output and ANSI escape sequences for colors on modern Windows consoles
static void enable_windows_utf8_and_ansi(){
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr){
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)){
            mode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
            SetConsoleMode(hOut, mode);
        }
    }
}
// No pause-on-exit; app provides an interactive prompt instead
#endif

namespace fs = std::filesystem;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Very tiny, permissive TOML-ish reader for flat key=value (string/number/bool) pairs.
// It's not a full TOML parser, but enough to detect config path and read some prefs.
static std::unordered_map<std::string, std::string> read_simple_kv(const fs::path &p) {
    std::unordered_map<std::string, std::string> kv;
    std::ifstream in(p);
    if (!in) return kv;
    std::string line;
    while (std::getline(in, line)) {
        // Strip comments
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        // Trim
        auto ltrim = [](std::string &s){ s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch){ return !std::isspace(ch); })); };
        auto rtrim = [](std::string &s){ s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch){ return !std::isspace(ch); }).base(), s.end()); };
        ltrim(line); rtrim(line);
        if (line.empty() || line[0]=='[') continue; // ignore sections
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq+1);
        ltrim(key); rtrim(key); ltrim(val); rtrim(val);
        if (!val.empty() && (val.front()=='"' || val.front()=='\'')) {
            if (val.size()>=2 && val.back()==val.front()) {
                val = val.substr(1, val.size()-2);
            }
        }
        kv[key] = val;
    }
    return kv;
}

static std::string now_local_iso() {
    using namespace std::chrono;
    auto t = system_clock::to_time_t(system_clock::now());
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

#if defined(_WIN32)
// Minimal IP-based geolocation using ipapi.co (no key). Returns lat,lon,tz,city,country strings.
static std::optional<std::tuple<double,double,std::string,std::string,std::string>> ip_geolocate(){
    HINTERNET hSession = WinHttpOpen(L"Almuslim/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return std::nullopt;
    HINTERNET hConnect = WinHttpConnect(hSession, L"ipapi.co", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect){ WinHttpCloseHandle(hSession); return std::nullopt; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/json/", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest){ WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return std::nullopt; }
    BOOL b = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!b){ WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return std::nullopt; }
    b = WinHttpReceiveResponse(hRequest, NULL);
    if (!b){ WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return std::nullopt; }
    std::string body; body.reserve(1024);
    DWORD dwSize = 0;
    do{
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;
        std::string buf; buf.resize(dwSize);
        DWORD dwRead = 0; if (!WinHttpReadData(hRequest, buf.data(), dwSize, &dwRead)) break;
        buf.resize(dwRead); body += buf;
    } while(dwSize > 0);
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    // naive parse for "latitude": and "longitude": and "timezone":
    auto find_num = [&](const char* key)->std::optional<double>{
        std::string k = std::string("\"") + key + "\":";
        auto p = body.find(k); if (p==std::string::npos) return std::nullopt; p += k.size();
        // skip spaces
        while (p<body.size() && (body[p]==' ')) ++p;
        size_t e = p; while (e<body.size() && (std::isdigit((unsigned char)body[e]) || body[e]=='-' || body[e]=='+' || body[e]=='.')) ++e;
        try{ return std::stod(body.substr(p, e-p)); }catch(...){ return std::nullopt; }
    };
    auto find_str = [&](const char* key)->std::optional<std::string>{
        std::string k = std::string("\"") + key + "\":\"";
        auto p = body.find(k); if (p==std::string::npos) return std::nullopt; p += k.size();
        auto e = body.find('"', p); if (e==std::string::npos) return std::nullopt;
        return body.substr(p, e-p);
    };
    auto lat = find_num("latitude");
    auto lon = find_num("longitude");
    auto tz = find_str("timezone");
    auto city = find_str("city");
    auto country = find_str("country_name");
    if (lat && lon){ return std::make_tuple(*lat, *lon, tz.value_or(""), city.value_or(""), country.value_or("")); }
    return std::nullopt;
}
#endif

// Very simple Hijri approximation (Umm al-Qura-like) for display only.
// For production-grade accuracy, replace with a true Umm al-Qura table-based conversion.
static std::string approx_hijri_date(const std::tm &lt){
    // Algorithm: Kuwaiti algorithm-style rough approximation
    int y = lt.tm_year + 1900;
    int m = lt.tm_mon + 1;
    int d = lt.tm_mday;
    // Julian Day Number (approx Gregorian to JDN)
    int a = (14 - m)/12;
    int y2 = y + 4800 - a;
    int m2 = m + 12*a - 3;
    long jdn = d + (153*m2 + 2)/5 + 365L*y2 + y2/4 - y2/100 + y2/400 - 32045;
    // Islamic date calculation (Tabular, 30-year cycle)
    long l = jdn - 1948439; // days since 1 Muharram 1 AH (approx)
    long hcycles = l / 10631; // 30-year cycles
    l %= 10631;
    long ycycle = (l - 0.1335) / 354.36667; // close fit
    if (ycycle < 0) ycycle = 0;
    long hy = 1 + (long)ycycle + 30*hcycles;
    long doy = l - (long)(std::floor((ycycle)*354.36667 + 0.5));
    if (doy < 0) doy = 0;
    // Months: alternates 30/29 roughly; 12 months per year
    int hm = 1; int hd = (int)doy + 1; // start counting day 1
    static const int ml[12] = {30,29,30,29,30,29,30,29,30,29,30,29};
    for (int i=0;i<12;i++){
        if (hd > ml[i]){ hd -= ml[i]; hm++; }
        else break;
    }
    const char* mnames[12] = {"Muharram","Safar","Rabi' I","Rabi' II","Jumada I","Jumada II","Rajab","Sha'ban","Ramadan","Shawwal","Dhul-Qa'dah","Dhul-Hijjah"};
    hm = std::min(std::max(hm,1),12);
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%d %s %ld AH", hd, mnames[hm-1], hy);
    return std::string(buf);
}

// Add or subtract whole days from a time_t, returning local tm
static std::tm add_days_local(std::tm base, int days){
    base.tm_isdst = -1; // let mktime decide
    time_t t = mktime(&base);
    if (t == (time_t)-1) {
        std::tm zero{}; return zero;
    }
    t += static_cast<time_t>(days) * 24 * 3600;
    std::tm out{};
#if defined(_WIN32)
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
    return out;
}

// Simple theming: none|light|dark|auto (auto=use dark) + optional 256-color fg/bg
struct Theme { bool color = true; bool dark = true; int fg = -1; int bg = -1; };
static Theme resolve_theme(const std::string &colors){
    std::string c = colors; std::string x; x.resize(c.size());
    std::transform(c.begin(), c.end(), x.begin(), [](unsigned char ch){ return (char)std::tolower(ch); });
    if (x == "none" || x == "off") return Theme{false, true};
    if (x == "light") return Theme{true, false};
    if (x == "dark") return Theme{true, true};
    // auto default to dark
    return Theme{true, true};
}
static std::string cstr(const Theme &t, const char* code){ return t.color ? std::string(code) : std::string(""); }
static std::string creset(const Theme &t){ return t.color ? std::string("\x1b[0m") : std::string(""); }
static std::string cbold(const Theme &t){ return t.color ? std::string("\x1b[1m") : std::string(""); }
static std::string cdim(const Theme &t){ return t.color ? std::string("\x1b[2m") : std::string(""); }
static std::string cfg(const Theme &t, int idx){
    if (!t.color) return "";
    char buf[16]; std::snprintf(buf, sizeof(buf), "\x1b[38;5;%dm", idx);
    return std::string(buf);
}
static std::string cbg(const Theme &t, int idx){
    if (!t.color) return "";
    char buf[16]; std::snprintf(buf, sizeof(buf), "\x1b[48;5;%dm", idx);
    return std::string(buf);
}
static std::string creset_bg(const Theme &t){
    if (!t.color) return "";
    std::string s = "\x1b[0m";
    if (t.bg>=0) s += cbg(t, t.bg);
    if (t.fg>=0) s += cfg(t, t.fg);
    return s;
}

// Bidirectional text helpers (Arabic):
// Use Right-to-Left Embedding (U+202B) and Pop Directional Formatting (U+202C)
static inline const char* bidi_RLE(){ return "\xE2\x80\xAB"; }
static inline const char* bidi_PDF(){ return "\xE2\x80\xAC"; }
static std::string rtl_wrap(const std::string &s){ return std::string(bidi_RLE()) + s + bidi_PDF(); }
static int parse_color_index(const std::string &nameOrIndex){
    if (nameOrIndex.empty()) return -1;
    std::string s = nameOrIndex; for(char &c: s) c = (char)std::tolower((unsigned char)c);
    // numeric
    bool allDigits = !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char ch){ return std::isdigit(ch); });
    if (allDigits){
        int v = 0; try{ v = std::stoi(s); }catch(...){ return -1; }
        if (v>=0 && v<=255) return v; else return -1;
    }
    // simple names -> 256-color palette suggestions
    if (s=="black") return 16; if (s=="white") return 231; if (s=="gray"||s=="grey") return 244;
    if (s=="red") return 196; if (s=="green") return 34; if (s=="blue") return 27;
    if (s=="purple"||s=="magenta") return 129; if (s=="teal"||s=="cyan") return 37;
    if (s=="orange") return 208; if (s=="yellow") return 226;
    if (s=="dark") return 235; if (s=="light") return 255;
    return -1;
}
static Theme build_theme(const std::string &colors, const std::string &fgS, const std::string &bgS){
    Theme t = resolve_theme(colors);
    t.fg = parse_color_index(fgS);
    t.bg = parse_color_index(bgS);
    // auto contrast if fg not set but bg is
    if (t.bg>=0 && t.fg<0){ t.fg = (t.bg<100? 231 : 16); }
    return t;
}
static void clear_screen(){ std::cout << "\x1b[2J\x1b[H"; }
static void apply_theme_colors(const Theme &t){
    if (!t.color) return;
    if (t.bg>=0) std::cout << cbg(t, t.bg);
    if (t.fg>=0) std::cout << cfg(t, t.fg);
}

// Math helpers
static inline double deg2rad(double d){ return d * M_PI / 180.0; }
static inline double rad2deg(double r){ return r * 180.0 / M_PI; }
static inline double clamp(double v, double lo, double hi){ return std::max(lo, std::min(hi, v)); }

// Draw a boxed table with Unicode line art
static void draw_boxed_table(const Theme& theme,
                             const std::vector<std::string>& leftCol,
                             const std::vector<std::string>& rightCol,
                             int highlightRow /*-1 = none*/, bool rtlNames /*=false*/ = false){
    size_t n = std::min(leftCol.size(), rightCol.size());
    size_t lw = 0, rw = 0;
    for (size_t i=0;i<n;i++){ lw = std::max(lw, leftCol[i].size()); rw = std::max(rw, rightCol[i].size()); }
    lw = std::min<size_t>(lw, 16); // cap a bit
    rw = std::max<size_t>(rw, 5);
    auto rep_s = [](const std::string& piece, size_t count){ std::string s; s.reserve(piece.size()*count); for(size_t i=0;i<count;++i) s += piece; return s; };
    const std::string h = "─"; // UTF-8 horizontal line
    std::string top = std::string("\x1b[0m\x1b[90m") + "┌" + rep_s(h, lw+2) + "┬" + rep_s(h, rw+2) + "┐" + creset_bg(theme);
    std::string mid = std::string("\x1b[0m\x1b[90m") + "├" + rep_s(h, lw+2) + "┼" + rep_s(h, rw+2) + "┤" + creset_bg(theme);
    std::string bot = std::string("\x1b[0m\x1b[90m") + "└" + rep_s(h, lw+2) + "┴" + rep_s(h, rw+2) + "┘" + creset_bg(theme);
    std::cout << top << "\n";
    // Header row (localized)
    {
        std::string l = rtlNames ? "الصلاة" : "Prayer"; std::string r = rtlNames ? "الوقت" : "Time";
        if (rtlNames){
            std::cout << "\x1b[90m│\x1b[0m " << cbold(theme) << cfg(theme, 45) << std::right << std::setw((int)lw) << l << creset_bg(theme)
                      << " \x1b[90m│\x1b[0m " << cbold(theme) << cfg(theme, 45) << std::setw((int)rw) << r << creset_bg(theme)
                      << " \x1b[90m│\x1b[0m\n";
        } else {
            std::cout << "\x1b[90m│\x1b[0m " << cbold(theme) << cfg(theme, 45) << std::left << std::setw((int)lw) << l << creset_bg(theme)
                      << " \x1b[90m│\x1b[0m " << cbold(theme) << cfg(theme, 45) << std::setw((int)rw) << r << creset_bg(theme)
                      << " \x1b[90m│\x1b[0m\n";
        }
    }
    std::cout << mid << "\n";
    for (size_t i=0;i<n;i++){
        bool hl = ((int)i == highlightRow);
        std::string pre = hl ? (cbold(theme) + cfg(theme, 82)) : std::string(""); // green highlight
        std::string suf = hl ? creset(theme) : std::string("");
        if (rtlNames){
            std::cout << "\x1b[90m│\x1b[0m "
                      << pre << std::right << std::setw((int)lw) << leftCol[i] << suf
                      << " \x1b[90m│\x1b[0m "
                      << pre << std::setw((int)rw) << rightCol[i] << suf
                      << " \x1b[90m│\x1b[0m\n";
        } else {
            std::cout << "\x1b[90m│\x1b[0m "
                      << pre << std::left << std::setw((int)lw) << leftCol[i] << suf
                      << " \x1b[90m│\x1b[0m "
                      << pre << std::setw((int)rw) << rightCol[i] << suf
                      << " \x1b[90m│\x1b[0m\n";
        }
        if (i+1==n) break;
    }
    std::cout << bot << "\n";
}

static void draw_progress_bar(const Theme& theme, double fraction, int width=30){
    fraction = std::max(0.0, std::min(1.0, fraction));
    int filled = (int)std::round(fraction * width);
    std::string bar = "[";
    for (int i=0;i<width;i++){
        if (i < filled) bar += "#"; else bar += "-";
    }
    bar += "]";
    std::cout << cdim(theme) << bar << creset(theme);
}

// Compute day of year
static int day_of_year(const std::tm &tm){
    static const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int yday = 0;
    for(int m=0;m<tm.tm_mon;m++) yday += mdays[m];
    yday += tm.tm_mday;
    int year = tm.tm_year + 1900;
    bool leap = ((year%4==0 && year%100!=0) || (year%400==0));
    if (leap && tm.tm_mon>1) yday += 1;
    return yday;
}

// NOAA-style solar calculations: equation of time (minutes) and declination (degrees)
static void solar_params_noaa(int yday, double &eqTimeMin, double &declDeg){
    // Fractional year in radians (approx)
    double gamma = 2.0*M_PI/365.0 * (yday - 1 + (12 - 12)/24.0);
    // Equation of time (minutes)
    double eq = 229.18*(0.000075 + 0.001868*cos(gamma) - 0.032077*sin(gamma)
                        - 0.014615*cos(2*gamma) - 0.040849*sin(2*gamma));
    // Solar declination (radians)
    double decl = 0.006918 - 0.399912*cos(gamma) + 0.070257*sin(gamma)
                  - 0.006758*cos(2*gamma) + 0.000907*sin(2*gamma)
                  - 0.002697*cos(3*gamma) + 0.00148*sin(3*gamma);
    eqTimeMin = eq;
    declDeg = decl * 180.0/M_PI;
}

// Compute local solar noon (hours, local clock) given longitude (deg), tzOffsetHours
static double solar_noon_local(double longitude, double tzOffsetHours, double eqTimeMin){
    // time offset in minutes between solar time and local time
    // true solar time minutes = local clock minutes + eqTime + 4*longitude - 60*tz
    // set true solar time to 720 (12:00) to get local clock time
    double localNoonMin = 720 - eqTimeMin - 4*longitude + 60*tzOffsetHours;
    return localNoonMin / 60.0; // hours
}

// Hour angle for a given solar altitude angle (deg). Returns degrees >=0.
static std::optional<double> hour_angle_deg(double latDeg, double declDeg, double altitudeDeg){
    double lat = deg2rad(latDeg);
    double decl = deg2rad(declDeg);
    double alt = deg2rad(altitudeDeg);
    double cosH = (std::sin(alt) - std::sin(lat)*std::sin(decl)) / (std::cos(lat)*std::cos(decl));
    if (cosH < -1.0 || cosH > 1.0) return std::nullopt;
    double H = std::acos(clamp(cosH, -1.0, 1.0)); // radians
    return rad2deg(H);
}

// Asr target altitude given madhab factor (1 for Shafi, 2 for Hanafi)
static double asr_altitude_deg(double latDeg, double declDeg, int factor){
    // Proper Asr altitude: alt = 90° - arctan(factor + tan(|phi - decl|))
    double lat = std::fabs(deg2rad(latDeg));
    double decl = std::fabs(deg2rad(declDeg));
    double alt = (M_PI/2.0) - std::atan(factor + std::tan(std::fabs(lat - decl)));
    return rad2deg(alt);
}

// Get local UTC offset (hours) for current time (approx for today)
static double local_utc_offset_hours(){
    using namespace std::chrono;
    auto t = system_clock::to_time_t(system_clock::now());
    std::tm lt{}; std::tm gt{};
#if defined(_WIN32)
    localtime_s(&lt, &t);
    gmtime_s(&gt, &t);
#else
    localtime_r(&t, &lt);
    gmtime_r(&t, &gt);
#endif
    // mktime treats struct as local time; we need seconds since epoch
    time_t l = mktime(&lt);
#if defined(_WIN32)
    // There is no portable timegm; approximate by difference
    time_t g = _mkgmtime(&gt);
#else
    time_t g = timegm(&gt);
#endif
    double diff = std::difftime(l, g); // seconds
    return diff / 3600.0;
}

struct PrayerTimes { double fajr, sunrise, dhuhr, asr, maghrib, isha; };

static std::optional<PrayerTimes> compute_prayer_times(const std::tm &date, double latitude, double longitude,
                                                       const std::string &method, const std::string &madhab,
                                                       const std::string &high_lat_rule,
                                                       std::optional<double> tzOverrideHours = std::nullopt){
    int yday = day_of_year(date);
    double eqMin=0.0, declDeg=0.0; solar_params_noaa(yday, eqMin, declDeg);
    double tz = tzOverrideHours.has_value() ? *tzOverrideHours : local_utc_offset_hours();
    double noon = solar_noon_local(longitude, tz, eqMin);

    // Method presets (angles in degrees, negative altitudes: below horizon)
    double fajrAngle = 18.0; // default
    double ishaAngle = 18.0; // default
    int ishaOffsetMin = -1;  // if >=0, use fixed minutes after Maghrib
    if (method == "isna") { fajrAngle = 15.0; ishaAngle = 15.0; }
    else if (method == "mwl") { fajrAngle = 18.0; ishaAngle = 17.0; }
    else if (method == "umm_al_qura" || method == "makkah") { fajrAngle = 18.5; ishaOffsetMin = 90; }
    else if (method == "egypt") { fajrAngle = 19.5; ishaAngle = 17.5; }
    else if (method == "karachi") { fajrAngle = 18.0; ishaAngle = 18.0; }
    else if (method == "tehran") { fajrAngle = 17.7; ishaAngle = 14.0; }
    // others can be added

    // Sunrise/Sunset standard altitude includes refraction and solar radius ≈ -0.833°
    auto Hsr = hour_angle_deg(latitude, declDeg, -0.833);
    if (!Hsr) return std::nullopt;
    double sunrise = noon - (*Hsr)/15.0;
    double sunset  = noon + (*Hsr)/15.0;

    // Fajr/Isha using angles below horizon
    auto Hf = hour_angle_deg(latitude, declDeg, -fajrAngle);
    std::optional<double> Hi;
    if (ishaOffsetMin < 0) {
        Hi = hour_angle_deg(latitude, declDeg, -ishaAngle);
    }

    // Handle high latitude basic rule: cap night portions
    if ((!Hf || (!Hi && ishaOffsetMin < 0)) && (high_lat_rule=="middle_of_the_night" || high_lat_rule=="seventh_of_the_night" || high_lat_rule=="twilight_angle")){
        double nightLen = (24.0 - sunset + sunrise); // hours from sunset to next sunrise
        double portion = 0.5; // middle_of_the_night
        if (high_lat_rule=="seventh_of_the_night") portion = 1.0/7.0;
        // twilight_angle proportional rule simplified: use angle/60 (~ rough)
        if (high_lat_rule=="twilight_angle") portion = std::max(fajrAngle, (ishaOffsetMin<0?ishaAngle:0.0)) / 60.0;
        double adj = portion * nightLen;
        if (!Hf) { Hf = 15.0 * (noon - (sunrise - adj)); }
        if (!Hi && ishaOffsetMin < 0) { Hi = 15.0 * ((sunset + adj) - noon); }
    }

    if (!Hf) return std::nullopt;
    double fajr = noon - (*Hf)/15.0;
    double isha = 0.0;
    if (ishaOffsetMin >= 0) {
        isha = sunset + (ishaOffsetMin/60.0);
    } else if (Hi) {
        isha = noon + (*Hi)/15.0;
    } else {
        // fallback if still missing
        isha = sunset + 1.5; // 90 minutes
    }

    // Dhuhr is solar noon (can add small offset of few minutes if desired)
    double dhuhr = noon + 0.0;

    // Asr
    int factor = (madhab=="hanafi"?2:1);
    double alt_asr = asr_altitude_deg(latitude, declDeg, factor);
    auto Ha = hour_angle_deg(latitude, declDeg, alt_asr);
    if (!Ha) return std::nullopt;
    double asr = noon + (*Ha)/15.0;

    PrayerTimes pt{fajr, sunrise, dhuhr, asr, sunset, isha};
    return pt;
}

static std::string fmt_time(double hours, bool use24h){
    if (hours < 0) hours += 24.0;
    if (hours >= 24.0) hours = std::fmod(hours, 24.0);
    int h = static_cast<int>(std::floor(hours + 1e-9));
    int m = static_cast<int>(std::floor((hours - h)*60.0 + 0.5));
    if (m==60){ h=(h+1)%24; m=0; }
    char buf[16];
    if (use24h) {
        std::snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
    } else {
        int hh = h%12; if (hh==0) hh=12;
        const char* ampm = (h<12?"AM":"PM");
        std::snprintf(buf, sizeof(buf), "%d:%02d %s", hh, m, ampm);
    }
    return std::string(buf);
}

// Localize ASCII digits to Arabic-Indic digits for Arabic UI
static std::string localize_digits_ar(const std::string &s){
    static const char* dig[10] = {"٠","١","٢","٣","٤","٥","٦","٧","٨","٩"};
    std::string out; out.reserve(s.size()*2);
    for (unsigned char ch : s){
        if (ch>='0' && ch<='9') out += dig[ch-'0']; else out.push_back((char)ch);
    }
    return out;
}

static double hours_since_midnight_local(){
    using namespace std::chrono;
    auto t = system_clock::to_time_t(system_clock::now());
    std::tm lt{};
#if defined(_WIN32)
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    return lt.tm_hour + lt.tm_min/60.0 + lt.tm_sec/3600.0;
}

static void ensure_parent_exists(const fs::path& p){
    std::error_code ec; fs::create_directories(p.parent_path(), ec);
}

static void write_or_update_config(const fs::path& p, const std::unordered_map<std::string, std::string>& updates){
    std::unordered_map<std::string, std::string> cfg = read_simple_kv(p);
    for (auto &kv : updates) cfg[kv.first] = kv.second;
    ensure_parent_exists(p);
    std::ofstream out(p);
    if (!out) return;
    for (auto &kv : cfg){ out << kv.first << " = " << kv.second << "\n"; }
}

// Render the main screen from current config without exiting (used by refresh commands)
static void render_main_view(const char* argv0, const fs::path& config, std::unordered_map<std::string, std::string>& cfg){
    auto get_raw = [&](const std::string &k)->std::string{
        auto it = cfg.find(k); return it==cfg.end()?std::string():it->second;
    };
    std::string lang = get_raw("language"); if (lang.empty()) lang = "en";
    std::string colors = get_raw("colors"); if (colors.empty()) colors = "auto";
    std::string fgS = get_raw("fg"); std::string bgS = get_raw("bg");
    Theme theme = build_theme(colors, fgS, bgS);

    clear_screen();
    apply_theme_colors(theme);
    // Header
    std::cout << cstr(theme, "\x1b[32m");
    std::cout << "   ○○○○○   ○○○○   ○○○○○    Almuslim\n";
    std::cout << "  ○      ○   ○   ○      ○   Fast Terminal Prayer Times\n";
    std::cout << "  ○   ◐   ○   ○   ○   ★  ○   (C++)\n";
    std::cout << "  ○      ○   ○   ○      ○\n";
    std::cout << "   ○○○○○     ○     ○○○○○\n";
    std::cout << creset(theme);
    apply_theme_colors(theme);
    std::cout << "Date/Time (local): " << now_local_iso() << "\n";
    if (!config.empty()) { std::cout << "Config: " << config.string() << (fs::exists(config)?" (found)":" (missing)") << "\n"; }

    // Helpers
    auto get = [&](const std::string &k, const std::string &def)->std::string{
        auto it = cfg.find(k); return it==cfg.end()?def:it->second;
    };
    bool ar = false; { std::string L=lang; std::transform(L.begin(),L.end(),L.begin(),::tolower); ar = (L=="ar"||L=="arabic"); }
    auto Lbl = [&](const char* en, const char* arLabel){ return ar ? std::string(arLabel) : std::string(en); };

    std::string city = get("city", "(unset)");
    std::string latS = get("latitude", "");
    std::string lonS = get("longitude", "");
    std::string method = get("method", "umm_al_qura");
    std::string madhab = get("madhab", "shafi");
    std::string hlr = get("high_latitude_rule", "middle_of_the_night");
    std::string tzS = get("timezone", "");
    std::string elevS = get("elevation_m", "");
    bool use24h = true; { auto v = get("24h","true"); std::string s=v; std::transform(s.begin(),s.end(),s.begin(),::tolower); use24h = (s=="true"||s=="1"||s=="yes"); }

    double latitude=0.0, longitude=0.0;
    if (!latS.empty()) latitude = std::stod(latS);
    if (!lonS.empty()) longitude = std::stod(lonS);
    double elevation_m = 0.0; if (!elevS.empty()){ try { elevation_m = std::stod(elevS); } catch(...) { elevation_m = 0.0; } }

    using namespace std::chrono;
    auto t = system_clock::to_time_t(system_clock::now());
    std::tm lt{};
#if defined(_WIN32)
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif

    auto parse_tz_hours = [](const std::string &s)->std::optional<double>{
        if (s.empty()) return std::nullopt; std::string x = s; for(char &c: x) c = (char)std::tolower((unsigned char)c);
        if (x=="utc" || x=="gmt" || x=="z") return 0.0;
        if (x=="asia/riyadh" || x=="asia/makkah" || x=="asia/jeddah") return 3.0;
        if (x.rfind("utc",0)==0) x = x.substr(3);
        if (x.rfind("gmt",0)==0) x = x.substr(3);
        x.erase(std::remove_if(x.begin(), x.end(), ::isspace), x.end()); if (x.empty()) return std::nullopt;
        int sign = 1; size_t i=0; if (x[0]=='+'){sign=1;i=1;} else if (x[0]=='-'){sign=-1;i=1;}
        size_t colon = x.find(':', i); try{
            if (colon==std::string::npos) { return sign * std::stod(x.substr(i)); }
            double h = std::stod(x.substr(i, colon-i)); double m = std::stod(x.substr(colon+1)); return sign * (h + m/60.0);
        } catch(...) { return std::nullopt; }
    };
    std::optional<double> tzOverride = parse_tz_hours(tzS);
    auto ptOpt = compute_prayer_times(lt, latitude, longitude, method, madhab, hlr, tzOverride);
    if (!ptOpt) { std::cout << "\nUnable to compute prayer times for your location/date.\n"; return; }
    PrayerTimes pt = *ptOpt;

    // Hijri
    fs::path exeDir = fs::path(argv0).parent_path();
    fs::path dataDir = exeDir / "data";
#if !defined(_WIN32)
    if (!fs::exists(dataDir / "hijri/umm_al_qura_month_starts.csv")) {
        fs::path sysData = "/usr/share/almuslim/data"; if (fs::exists(sysData / "hijri/umm_al_qura_month_starts.csv")) dataDir = sysData;
    }
#endif
    static bool hjLoaded2 = hijri::load_umm_al_qura(dataDir);
    std::string hijriStr;
    if (hjLoaded2){ auto hd = hijri::hijri_for_date(lt); if (hd){ const char* mname = ar ? hijri::month_name_ar(hd->month) : hijri::month_name_en(hd->month); char buf[128]; std::snprintf(buf, sizeof(buf), "%d %s %d AH", hd->day, mname, hd->year); hijriStr = buf; } }
    if (hijriStr.empty()) hijriStr = approx_hijri_date(lt);
    if (ar) hijriStr = localize_digits_ar(hijriStr);
    std::cout << "\n" << cstr(theme, "\x1b[36m") << (ar ? rtl_wrap(hijriStr) : hijriStr) << creset(theme) << "\n"; apply_theme_colors(theme);

    // Summary + boxed table
    {
        std::string sum = std::string("\n") + cdim(theme) + Lbl("City","المدينة") + ": " + creset(theme) + city
                        + "  " + cdim(theme) + Lbl("Method","الطريقة") + ": " + creset(theme) + method + " (" + madhab + ")\n";
        bool arSum = false; { std::string L=lang; std::transform(L.begin(),L.end(),L.begin(),::tolower); arSum = (L=="ar"||L=="arabic"); }
        std::cout << (arSum ? rtl_wrap(sum) : sum);
    }
    std::vector<std::string> names = { Lbl("Fajr","الفجر"), Lbl("Sunrise","الشروق"), Lbl("Dhuhr","الظهر"), Lbl("Asr","العصر"), Lbl("Maghrib","المغرب"), Lbl("Isha","العشاء") };
    std::vector<std::string> timesV = { fmt_time(pt.fajr, use24h), fmt_time(pt.sunrise, use24h), fmt_time(pt.dhuhr, use24h), fmt_time(pt.asr, use24h), fmt_time(pt.maghrib, use24h), fmt_time(pt.isha, use24h) };
    if (ar){ for (auto &x : timesV) x = localize_digits_ar(x); }
    int nextIdx = -1; { double nowHtmp = hours_since_midnight_local(); std::vector<double> seqH = {pt.fajr, pt.sunrise, pt.dhuhr, pt.asr, pt.maghrib, pt.isha}; for (int i=0;i<(int)seqH.size();++i){ if (seqH[i] - nowHtmp >= -0.0001){ nextIdx = i; break; }} if (nextIdx < 0) nextIdx = 0; }
    draw_boxed_table(theme, names, timesV, nextIdx, ar);

    // Day length
    double dayLenH = pt.maghrib - pt.sunrise; if (dayLenH < 0) dayLenH += 24.0; int dlh = (int)std::floor(dayLenH + 1e-9); int dlm = (int)std::floor((dayLenH - dlh)*60.0 + 0.5); if (dlm==60){dlh+=1;dlm=0;}
    char dBuf[32]; std::snprintf(dBuf, sizeof(dBuf), "%02d:%02d", std::max(0,dlh), std::max(0,dlm)); std::string dStr = dBuf; if (ar) dStr = localize_digits_ar(dStr);
    {
        std::string line = Lbl("Day length","طول النهار") + std::string(": ") + dStr + "\n";
        std::cout << (ar ? rtl_wrap(line) : line);
    }

    // Next prayer with progress
    double nowH = hours_since_midnight_local(); std::vector<std::pair<std::string,double>> seq = {{"Fajr", pt.fajr},{"Sunrise", pt.sunrise},{"Dhuhr", pt.dhuhr},{"Asr", pt.asr},{"Maghrib", pt.maghrib},{"Isha", pt.isha}};
    std::string nextName = ""; double nextInH = 0.0; for (auto &p: seq){ double dt = p.second - nowH; if (dt < -0.0001) continue; nextName = p.first; nextInH = dt; break; } if (nextName.empty()) { nextName = seq.front().first; nextInH = (24.0 - nowH) + seq.front().second; }
    if (ar){ if (nextName=="Fajr") nextName="الفجر"; else if (nextName=="Sunrise") nextName="الشروق"; else if (nextName=="Dhuhr") nextName="الظهر"; else if (nextName=="Asr") nextName="العصر"; else if (nextName=="Maghrib") nextName="المغرب"; else if (nextName=="Isha") nextName="العشاء"; }
    int h = (int)std::floor(nextInH + 1e-9); int m = (int)std::floor((nextInH - h)*60.0 + 0.5); if (m==60){ h+=1; m=0; } char buf2[32]; std::snprintf(buf2, sizeof(buf2), "%02d:%02d", std::max(0,h), std::max(0,m)); std::string nextStr = buf2; if (ar) nextStr = localize_digits_ar(nextStr);
    double prevT=0.0, nextT=0.0, nowH2=hours_since_midnight_local(); { int idx = nextIdx; int prevIdx = (idx-1>=0?idx-1:(int)seq.size()-1); prevT = seq[prevIdx].second; nextT = seq[idx].second; if (nowH2 < prevT) nowH2 += 24.0; if (nextT < prevT) nextT += 24.0; }
    double frac = (nowH2 - prevT) / std::max(0.001, (nextT - prevT));
    {
        std::string line = std::string("\n") + Lbl("Next","التالي") + " (" + nextName + ") " + Lbl("in","بعد") + ": " + nextStr + "  ";
        std::cout << (ar ? rtl_wrap(line) : line);
    }
    draw_progress_bar(theme, frac); std::cout << "\n";
}
// Guided onboarding on first run: welcome, city, language, clock, background
static void onboarding_wizard(const fs::path& config, const fs::path& exeDir){
    // Styles
    Theme t = build_theme("dark", "231", "23"); // greenish bg for onboarding
    clear_screen(); apply_theme_colors(t);
    std::cout << cbold(t) << "\n\n    Welcome to Almuslim" << creset(t) << "\n\n";
    std::cout << "This wizard will help you set up your city and preferences.\n";
    std::cout << "Press Enter to start..."; std::string tmp; std::getline(std::cin, tmp);

    // Load cities
    fs::path dataDir = exeDir / "data";
#if !defined(_WIN32)
    if (!fs::exists(dataDir / "cities.csv")) {
        fs::path sysData = "/usr/share/almuslim/data";
        if (fs::exists(sysData / "cities.csv")) dataDir = sysData;
    }
#endif
    auto cities = load_cities(dataDir);
    std::optional<City> chosen = prompt_city_free_text(cities);
    if (!chosen){ std::cout << "Setup cancelled.\n"; return; }

    // Language
    std::string language = "en";
    std::cout << "\nLanguage? [en/ar] (default en): ";
    std::getline(std::cin, tmp); if (!tmp.empty()){
        std::string tl = tmp; for(char &c: tl) c=(char)std::tolower((unsigned char)c);
        if (tl=="ar"||tl=="arabic") language = "ar";
    }
    // Clock
    std::string use24 = "true";
    std::cout << "24-hour clock? [Y/n] (default Y): ";
    std::getline(std::cin, tmp); if (!tmp.empty()){
        std::string tl = tmp; for(char &c: tl) c=(char)std::tolower((unsigned char)c);
        if (tl=="n"||tl=="no") use24 = "false";
    }
    // Method and Madhab
    std::cout << "\nCalculation method? [umm_al_qura|mwl|isna|egypt|karachi|tehran] (default umm_al_qura): ";
    std::string method = "umm_al_qura"; std::getline(std::cin, tmp); if (!tmp.empty()) method = tmp;
    std::cout << "Madhab? [shafi|hanafi] (default shafi): ";
    std::string madhab = "shafi"; std::getline(std::cin, tmp); if (!tmp.empty()) madhab = tmp;

    // Background
    std::cout << "\nPick a background color (name or 0-255), examples: dark, blue, green, purple, teal, orange, none\n> ";
    std::string bg = ""; std::getline(std::cin, bg);
    if (bg=="none"||bg=="off") bg.clear();

    const City &c = *chosen;
    auto q = [](const std::string &s){ return '"' + s + '"'; };
    std::unordered_map<std::string,std::string> updates;
    updates["city"] = q(c.name + ", " + c.country);
    updates["latitude"] = std::to_string(c.lat);
    updates["longitude"] = std::to_string(c.lon);
    updates["timezone"] = q(c.tz);
    updates["method"] = q(method);
    updates["madhab"] = q(madhab);
    updates["high_latitude_rule"] = q("middle_of_the_night");
    updates["24h"] = q(use24);
    if (!bg.empty()) updates["bg"] = q(bg);
    updates["language"] = q(language);
    write_or_update_config(config, updates);
    std::cout << "\nSaved config to: " << config.string() << "\n\n";
}

int main(int argc, char** argv) {
    try {
#if defined(_WIN32)
        enable_windows_utf8_and_ansi();
#endif
        bool askEveryLaunch = false;
        bool showWeek = false;
        std::optional<std::string> weekCsvPath;
        bool detectLocation = false; // future hook
        for (int i=1;i<argc;i++){
            std::string a = argv[i];
            if (a == "--ask") askEveryLaunch = true;
            if (a == "--week") showWeek = true;
            if (a == "--week-csv" && i+1 < argc) { weekCsvPath = std::string(argv[++i]); }
            if (a == "--detect-location") detectLocation = true;
        }
        // Resolve config path
        fs::path config = platform::resolve_config_path();
        std::unordered_map<std::string, std::string> cfg;
        if (!config.empty() && fs::exists(config)) {
            cfg = read_simple_kv(config);
        } else {
            // First run onboarding
            fs::path exeDir = fs::path(argv[0]).parent_path();
            onboarding_wizard(config, exeDir);
            if (fs::exists(config)) cfg = read_simple_kv(config);
        }

        // Setup mode to select city interactively and write config
        if (argc > 1 && std::string(argv[1]) == "--setup"){
            fs::path exeDir = fs::path(argv[0]).parent_path();
            fs::path dataDir = exeDir / "data";
#if !defined(_WIN32)
            if (!fs::exists(dataDir / "cities.csv")) {
                fs::path sysData = "/usr/share/almuslim/data";
                if (fs::exists(sysData / "cities.csv")) dataDir = sysData;
            }
#endif
            auto cities = load_cities(dataDir);
            if (cities.empty()){
                std::cout << "No cities database found at " << (dataDir/"cities.csv").string() << "\n";
                return 1;
            }
            // Free-text input for more professional UX
            auto chosen = prompt_city_free_text(cities);
            if (!chosen){ std::cout << "Setup cancelled.\n"; return 0; }
            const City &c = *chosen;
            std::unordered_map<std::string,std::string> updates;
            auto q = [](const std::string &s){ return '"' + s + '"'; };
            updates["city"] = q(c.name + ", " + c.country);
            updates["latitude"] = std::to_string(c.lat);
            updates["longitude"] = std::to_string(c.lon);
            updates["timezone"] = q(c.tz);
            updates["method"] = q("umm_al_qura");
            updates["madhab"] = q("shafi");
            updates["high_latitude_rule"] = q("middle_of_the_night");
            updates["24h"] = q("true");
            write_or_update_config(config, updates);
            std::cout << "Saved config to: " << config.string() << "\n";
            // Continue to print today's times for chosen city
            cfg = read_simple_kv(config);
        }

        // Resolve theme and language from config early (fallbacks below)
        auto get_raw = [&](const std::string &k)->std::string{
            auto it = cfg.find(k); return it==cfg.end()?std::string():it->second;
        };
    std::string lang = get_raw("language"); if (lang.empty()) lang = "en";
    std::string colors = get_raw("colors"); if (colors.empty()) colors = "auto";
    std::string fgS = get_raw("fg");
    std::string bgS = get_raw("bg");
    Theme theme = build_theme(colors, fgS, bgS);

    // Apply background/foreground if set
    clear_screen();
    apply_theme_colors(theme);

    // Professional header with ASCII rendition of the logo
    std::cout << cstr(theme, "\x1b[32m"); // green
        std::cout << "   ○○○○○   ○○○○   ○○○○○    Almuslim\n";
        std::cout << "  ○      ○   ○   ○      ○   Fast Terminal Prayer Times\n";
        std::cout << "  ○   ◐   ○   ○   ○   ★  ○   (C++)\n";
        std::cout << "  ○      ○   ○   ○      ○\n";
        std::cout << "   ○○○○○     ○     ○○○○○\n";
    std::cout << creset(theme);
    apply_theme_colors(theme);
        std::cout << "Date/Time (local): " << now_local_iso() << "\n";
        if (!config.empty()) {
            std::cout << "Config: " << config.string() << (fs::exists(config)?" (found)":" (missing)") << "\n";
        }

        // Read a few known keys (flat)
        auto get = [&](const std::string &k, const std::string &def)->std::string{
            auto it = cfg.find(k);
            return it==cfg.end()?def:it->second;
        };

        std::string city = get("city", "(unset)");
        std::string latS = get("latitude", "");
        std::string lonS = get("longitude", "");
        std::string method = get("method", "umm_al_qura");
        std::string madhab = get("madhab", "shafi");
    std::string hlr = get("high_latitude_rule", "middle_of_the_night");
    std::string tzS = get("timezone", "");
    std::string elevS = get("elevation_m", "");
    bool use24h = true; { auto v = get("24h","true"); std::string s=v; std::transform(s.begin(),s.end(),s.begin(),::tolower); use24h = (s=="true"||s=="1"||s=="yes"); }
    // ask_on_start in config (optional)
    { auto v = get("ask_on_start","false"); std::string s=v; std::transform(s.begin(),s.end(),s.begin(),::tolower); if (s=="true"||s=="1"||s=="yes") askEveryLaunch = true; }

        // Ask for city each launch if requested or if not set
        if (askEveryLaunch || city == "(unset)" || latS.empty() || lonS.empty()){
            fs::path exeDir = fs::path(argv[0]).parent_path();
            fs::path dataDir = exeDir / "data";
#if !defined(_WIN32)
            if (!fs::exists(dataDir / "cities.csv")) {
                fs::path sysData = "/usr/share/almuslim/data";
                if (fs::exists(sysData / "cities.csv")) dataDir = sysData;
            }
#endif
            auto cities = load_cities(dataDir);
            if (!cities.empty()){
                auto chosen = prompt_city_free_text(cities);
                if (chosen){
                    const City &c = *chosen;
                    city = c.name + ", " + c.country;
                    latS = std::to_string(c.lat);
                    lonS = std::to_string(c.lon);
                    tzS = c.tz;
                    // persist
                    std::unordered_map<std::string,std::string> updates;
                    auto q = [](const std::string &s){ return '"' + s + '"'; };
                    updates["city"] = q(city);
                    updates["latitude"] = latS;
                    updates["longitude"] = lonS;
                    updates["timezone"] = q(tzS);
                    write_or_update_config(config, updates);
                }
            }
        }

    // Labels (English/Arabic)
    bool ar = false; { std::string L=lang; std::transform(L.begin(),L.end(),L.begin(),::tolower); ar = (L=="ar"||L=="arabic"); }
    auto Lbl = [&](const char* en, const char* arLabel){ return ar ? std::string(arLabel) : std::string(en); };

    if (!ar){
      std::cout << Lbl("City", "المدينة") << ": " << city << "\n";
      std::cout << Lbl("Latitude", "خط العرض") << ": " << (latS.empty()?"(unset)":latS) << ", "
          << Lbl("Longitude", "خط الطول") << ": " << (lonS.empty()?"(unset)":lonS) << "\n";
      std::cout << Lbl("Method", "الطريقة") << ": " << method << ", " << Lbl("Madhab", "المذهب") << ": " << madhab << "\n";
      std::cout << Lbl("High-latitude", "خطوط العرض العليا") << ": " << hlr << "\n";
    } else {
      std::cout << rtl_wrap(Lbl("City", "المدينة") + std::string(": ") + city + "\n");
      std::cout << rtl_wrap(Lbl("Latitude", "خط العرض") + std::string(": ") + (latS.empty()?"(unset)":latS) + ", "
          + Lbl("Longitude", "خط الطول") + ": " + (lonS.empty()?"(unset)":lonS) + "\n");
      std::cout << rtl_wrap(Lbl("Method", "الطريقة") + std::string(": ") + method + ", " + Lbl("Madhab", "المذهب") + ": " + madhab + "\n");
      std::cout << rtl_wrap(Lbl("High-latitude", "خطوط العرض العليا") + std::string(": ") + hlr + "\n");
    }

        double latitude=0.0, longitude=0.0;
        if (!latS.empty()) latitude = std::stod(latS);
        if (!lonS.empty()) longitude = std::stod(lonS);
        double elevation_m = 0.0; if (!elevS.empty()){
            try { elevation_m = std::stod(elevS); } catch(...) { elevation_m = 0.0; }
        }

        using namespace std::chrono;
        auto t = system_clock::to_time_t(system_clock::now());
        std::tm lt{};
#if defined(_WIN32)
        localtime_s(&lt, &t);
#else
        localtime_r(&t, &lt);
#endif

        auto parse_tz_hours = [](const std::string &s)->std::optional<double>{
            if (s.empty()) return std::nullopt;
            std::string x = s; for(char &c: x) c = (char)std::tolower((unsigned char)c);
            if (x=="utc" || x=="gmt" || x=="z") return 0.0;
            if (x=="asia/riyadh" || x=="asia/makkah" || x=="asia/jeddah") return 3.0;
            if (x.rfind("utc",0)==0) x = x.substr(3);
            if (x.rfind("gmt",0)==0) x = x.substr(3);
            x.erase(std::remove_if(x.begin(), x.end(), ::isspace), x.end());
            if (x.empty()) return std::nullopt;
            int sign = 1; size_t i=0; if (x[0]=='+'){sign=1;i=1;} else if (x[0]=='-'){sign=-1;i=1;}
            size_t colon = x.find(':', i);
            try{
                if (colon==std::string::npos) { return sign * std::stod(x.substr(i)); }
                double h = std::stod(x.substr(i, colon-i));
                double m = std::stod(x.substr(colon+1));
                return sign * (h + m/60.0);
            } catch(...) { return std::nullopt; }
        };

        std::optional<double> tzOverride = parse_tz_hours(tzS);
        auto ptOpt = compute_prayer_times(lt, latitude, longitude, method, madhab, hlr, tzOverride);
        if (!ptOpt) {
            std::cout << "\nUnable to compute prayer times for your location/date (high-latitude or invalid coords).\n";
            return 0;
        }
        PrayerTimes pt = *ptOpt;

        // Hijri date: prefer precise table if available
    fs::path exeDir = fs::path(argv[0]).parent_path();
    fs::path dataDir = exeDir / "data";
#if !defined(_WIN32)
    if (!fs::exists(dataDir / "hijri/umm_al_qura_month_starts.csv")) {
        fs::path sysData = "/usr/share/almuslim/data";
        if (fs::exists(sysData / "hijri/umm_al_qura_month_starts.csv")) dataDir = sysData;
    }
#endif
    static bool hjLoaded = hijri::load_umm_al_qura(dataDir);
        std::string hijriStr;
        if (hjLoaded){
            auto hd = hijri::hijri_for_date(lt);
            if (hd){
                bool ar = false; { std::string L=lang; std::transform(L.begin(),L.end(),L.begin(),::tolower); ar = (L=="ar"||L=="arabic"); }
                const char* mname = ar ? hijri::month_name_ar(hd->month) : hijri::month_name_en(hd->month);
                char buf[128]; std::snprintf(buf, sizeof(buf), "%d %s %d AH", hd->day, mname, hd->year);
                hijriStr = buf;
            }
        }
        if (hijriStr.empty()){
            hijriStr = approx_hijri_date(lt);
        }
    if (ar) hijriStr = localize_digits_ar(hijriStr);
    bool ar2 = false; { std::string L=lang; std::transform(L.begin(),L.end(),L.begin(),::tolower); ar2 = (L=="ar"||L=="arabic"); }
    std::cout << "\n" << cstr(theme, "\x1b[36m") << (ar2 ? rtl_wrap(hijriStr) : hijriStr) << creset(theme) << "\n";

    // Fancy boxed table for prayers
    std::vector<std::string> names = {
        Lbl("Fajr","الفجر"), Lbl("Sunrise","الشروق"), Lbl("Dhuhr","الظهر"),
        Lbl("Asr","العصر"), Lbl("Maghrib","المغرب"), Lbl("Isha","العشاء")
    };
    std::vector<std::string> timesV = {
        fmt_time(pt.fajr, use24h), fmt_time(pt.sunrise, use24h), fmt_time(pt.dhuhr, use24h),
        fmt_time(pt.asr, use24h), fmt_time(pt.maghrib, use24h), fmt_time(pt.isha, use24h)
    };
    // Determine next prayer index for highlighting
    int nextIdx = -1;
    {
        double nowHtmp = hours_since_midnight_local();
        std::vector<double> seqH = {pt.fajr, pt.sunrise, pt.dhuhr, pt.asr, pt.maghrib, pt.isha};
        for (int i=0;i<(int)seqH.size();++i){ if (seqH[i] - nowHtmp >= -0.0001){ nextIdx = i; break; }}
        if (nextIdx < 0) nextIdx = 0; // wrap
    }
    {
        std::string sum = std::string("") + cdim(theme) + Lbl("City","المدينة") + ": " + creset(theme) + city + "  "
                        + cdim(theme) + Lbl("Method","الطريقة") + ": " + creset(theme) + method + " (" + madhab + ")\n";
        bool arSum = false; { std::string L=lang; std::transform(L.begin(),L.end(),L.begin(),::tolower); arSum = (L=="ar"||L=="arabic"); }
        std::cout << (arSum ? rtl_wrap(sum) : sum);
    }
    if (ar){ for (auto &x: timesV) x = localize_digits_ar(x); }
    draw_boxed_table(theme, names, timesV, nextIdx, ar);

        // Extra: Day length info
        double dayLenH = pt.maghrib - pt.sunrise; if (dayLenH < 0) dayLenH += 24.0;
        int dlh = (int)std::floor(dayLenH + 1e-9);
        int dlm = (int)std::floor((dayLenH - dlh)*60.0 + 0.5); if (dlm==60){dlh+=1;dlm=0;}
        char dBuf[32]; std::snprintf(dBuf, sizeof(dBuf), "%02d:%02d", std::max(0,dlh), std::max(0,dlm)); std::string dStr = dBuf; if (ar) dStr = localize_digits_ar(dStr);
        {
            std::string line = Lbl("Day length","طول النهار") + std::string(": ") + dStr + "\n";
            std::cout << (ar ? rtl_wrap(line) : line);
        }

        // Next prayer countdown
        double nowH = hours_since_midnight_local();
        std::vector<std::pair<std::string,double>> seq = {
            {"Fajr", pt.fajr}, {"Sunrise", pt.sunrise}, {"Dhuhr", pt.dhuhr}, {"Asr", pt.asr}, {"Maghrib", pt.maghrib}, {"Isha", pt.isha}
        };
        std::string nextName = ""; double nextInH = 0.0;
        for (auto &p: seq){
            double dt = p.second - nowH; if (dt < -0.0001) continue; nextName = p.first; nextInH = dt; break;
        }
        if (nextName.empty()) { nextName = seq.front().first; nextInH = (24.0 - nowH) + seq.front().second; }
        int h = static_cast<int>(std::floor(nextInH + 1e-9));
        int m = static_cast<int>(std::floor((nextInH - h)*60.0 + 0.5));
        if (m==60){ h+=1; m=0; }
    char buf[32]; std::snprintf(buf, sizeof(buf), "%02d:%02d", std::max(0,h), std::max(0,m)); std::string nextStr = buf; if (ar) nextStr = localize_digits_ar(nextStr);
        // Progress bar: fraction until the next prayer relative to previous to next
        double prevT = 0.0, nextT = 0.0, nowH2 = hours_since_midnight_local();
        {
            std::vector<std::pair<std::string,double>> seq2 = {
                {"Fajr", pt.fajr}, {"Sunrise", pt.sunrise}, {"Dhuhr", pt.dhuhr}, {"Asr", pt.asr}, {"Maghrib", pt.maghrib}, {"Isha", pt.isha}
            };
            int idx = nextIdx;
            int prevIdx = (idx-1>=0?idx-1:(int)seq2.size()-1);
            prevT = seq2[prevIdx].second; nextT = seq2[idx].second;
            if (nowH2 < prevT) nowH2 += 24.0; // wrap midnight
            if (nextT < prevT) nextT += 24.0;
        }
        double frac = (nowH2 - prevT) / std::max(0.001, (nextT - prevT));
    if (ar){ if (nextName=="Fajr") nextName="الفجر"; else if (nextName=="Sunrise") nextName="الشروق"; else if (nextName=="Dhuhr") nextName="الظهر"; else if (nextName=="Asr") nextName="العصر"; else if (nextName=="Maghrib") nextName="المغرب"; else if (nextName=="Isha") nextName="العشاء"; }
    {
        std::string line = std::string("\n") + Lbl("Next","التالي") + " (" + nextName + ") " + Lbl("in","بعد") + ": " + nextStr + "  ";
        std::cout << (ar ? rtl_wrap(line) : line);
    }
        draw_progress_bar(theme, frac);
        std::cout << "\n";
    {
        bool arTip = false; { std::string L=lang; std::transform(L.begin(),L.end(),L.begin(),::tolower); arTip = (L=="ar"||L=="arabic"); }
        std::string tipEn = "run with --setup for first-time setup, or --ask to choose a city on each launch.";
        std::string tipAr = "استخدم --setup للإعداد لأول مرة، أو --ask لاختيار المدينة عند كل تشغيل.";
        std::string line = Lbl("Tip","معلومة") + std::string(": ") + (arTip?tipAr:tipEn) + "\n";
        std::cout << (arTip ? rtl_wrap(line) : line);
    }

    // Weekly schedule (one-shot)
    if (showWeek || weekCsvPath.has_value()) {
            std::cout << "\n" << Lbl("Next 7 days","السبعة أيام القادمة") << ":\n";
            std::cout << "---------------------------------------------\n";
            std::ofstream csv;
            if (weekCsvPath){ csv.open(*weekCsvPath, std::ios::out | std::ios::trunc); if (csv) csv << "date,fajr,sunrise,dhuhr,asr,maghrib,isha\n"; }
            for (int i=0;i<7;i++){
                std::tm dt = add_days_local(lt, i);
                auto pt2 = compute_prayer_times(dt, latitude, longitude, method, madhab, hlr, tzOverride);
                if (!pt2) continue;
                char dstr[32]; std::snprintf(dstr, sizeof(dstr), "%04d-%02d-%02d", dt.tm_year+1900, dt.tm_mon+1, dt.tm_mday);
                std::cout << dstr << " | "
                          << Lbl("Fajr","فجر") << ": " << fmt_time(pt2->fajr, use24h) << ", "
                          << Lbl("Dhuhr","ظهر") << ": " << fmt_time(pt2->dhuhr, use24h) << ", "
                          << Lbl("Asr","عصر") << ": " << fmt_time(pt2->asr, use24h) << ", "
                          << Lbl("Maghrib","مغرب") << ": " << fmt_time(pt2->maghrib, use24h) << ", "
                          << Lbl("Isha","عشاء") << ": " << fmt_time(pt2->isha, use24h)
                          << "\n";
                if (csv){
                    csv << dstr << ","
                        << fmt_time(pt2->fajr, true) << ","
                        << fmt_time(pt2->sunrise, true) << ","
                        << fmt_time(pt2->dhuhr, true) << ","
                        << fmt_time(pt2->asr, true) << ","
                        << fmt_time(pt2->maghrib, true) << ","
                        << fmt_time(pt2->isha, true) << "\n";
                }
            }
            std::cout << "---------------------------------------------\n";
            if (csv){ std::cout << Lbl("CSV written to","تم حفظ CSV في") << ": " << *weekCsvPath << "\n"; }
        }

        // Interactive prompt for user-friendly commands
        auto print_help = [&](){
            bool ar = false; { std::string L=lang; std::transform(L.begin(),L.end(),L.begin(),::tolower); ar = (L=="ar"||L=="arabic"); }
            if (!ar){
                std::cout << "\nCommands: \n"
                          << "  help            Show this help\n"
                          << "  setup           Run onboarding to select city and preferences\n"
                          << "  ask             Choose a city for this session only\n"
                          << "  city <name>     Set city by name from database\n"
                          << "  week            Show next 7 days\n"
                          << "  detect          Try to auto-detect location (IP-based)\n"
                          << "  coords          Set latitude/longitude[/timezone]\n"
                          << "  refresh|r       Redraw and update now\n"
                          << "  quit|exit       Exit the app\n";
            } else {
                auto out = [&](const std::string &s){ std::cout << rtl_wrap(s); };
                out("\nالأوامر:\n");
                out("  help | مساعدة     عرض هذه المساعدة\n");
                out("  setup | إعداد     تشغيل معالج الإعداد\n");
                out("  ask | اختيار      اختيار مدينة للجلسة الحالية\n");
                out("  city <name> | مدينة <الاسم>  تعيين المدينة من قاعدة البيانات\n");
                out("  week | اسبوع      عرض ٧ أيام القادمة\n");
                out("  detect | كشف      محاولة تحديد الموقع (عن طريق IP)\n");
                out("  coords | إحداثيات  تعيين خط العرض/الطول [/المنطقة الزمنية]\n");
                out("  refresh | تحديث   إعادة التحديث الآن\n");
                out("  quit | exit | خروج  إنهاء التطبيق\n");
            }
        };
        std::string cmd;
        print_help();
        while (true){
            std::cout << "\n> ";
            if (!std::getline(std::cin, cmd)) break;
            std::string c = cmd; for(char &ch: c) ch=(char)std::tolower((unsigned char)ch);
            if (c=="" || c=="help" || c=="h" || c=="?" || c=="" || c=="مساعدة"){
                print_help();
            } else if (c=="setup" || c=="إعداد"){
                fs::path exeDir = fs::path(argv[0]).parent_path();
                onboarding_wizard(config, exeDir);
                cfg = read_simple_kv(config);
                render_main_view(argv[0], config, cfg);
                continue;
            } else if (c=="ask" || c=="اختيار"){
                fs::path exeDir = fs::path(argv[0]).parent_path();
                fs::path dataDir = exeDir / "data";
#if !defined(_WIN32)
                if (!fs::exists(dataDir / "cities.csv")) { fs::path sysData = "/usr/share/almuslim/data"; if (fs::exists(sysData / "cities.csv")) dataDir = sysData; }
#endif
                auto cities = load_cities(dataDir);
                auto chosen = prompt_city_free_text(cities);
                if (chosen){
                    const City &c0 = *chosen;
                    std::unordered_map<std::string,std::string> updates;
                    auto q = [](const std::string &s){ return '"' + s + '"'; };
                    updates["city"] = q(c0.name + ", " + c0.country);
                    updates["latitude"] = std::to_string(c0.lat);
                    updates["longitude"] = std::to_string(c0.lon);
                    updates["timezone"] = q(c0.tz);
                    write_or_update_config(config, updates);
                    std::cout << "Saved city to config.\n";
                    cfg = read_simple_kv(config);
                    render_main_view(argv[0], config, cfg);
                    continue;
                } else {
                    std::cout << "No match found. Type the city name to display anyway (or blank to cancel): ";
                    std::string cname; std::getline(std::cin, cname);
                    if (!cname.empty()){
                        std::unordered_map<std::string,std::string> up2; auto q = [](const std::string &s){ return '"' + s + '"'; };
                        up2["city"] = q(cname);
                        write_or_update_config(config, up2);
                        std::cout << "Saved custom city name.\n";
                        cfg = read_simple_kv(config);
                        render_main_view(argv[0], config, cfg);
                        continue;
                    }
                }
            } else if (c=="week" || c=="اسبوع"){
                // Quick week reprint
                std::cout << "\n---------------------------------------------\n";
                for (int i=0;i<7;i++){
                    std::tm dt = add_days_local(lt, i);
                    auto pt2 = compute_prayer_times(dt, latitude, longitude, method, madhab, hlr, tzOverride);
                    if (!pt2) continue;
                    char dstr[32]; std::snprintf(dstr, sizeof(dstr), "%04d-%02d-%02d", dt.tm_year+1900, dt.tm_mon+1, dt.tm_mday);
                    std::cout << dstr << " | "
                              << Lbl("Fajr","فجر") << ": " << fmt_time(pt2->fajr, use24h) << ", "
                              << Lbl("Dhuhr","ظهر") << ": " << fmt_time(pt2->dhuhr, use24h) << ", "
                              << Lbl("Asr","عصر") << ": " << fmt_time(pt2->asr, use24h) << ", "
                              << Lbl("Maghrib","مغرب") << ": " << fmt_time(pt2->maghrib, use24h) << ", "
                              << Lbl("Isha","عشاء") << ": " << fmt_time(pt2->isha, use24h)
                              << "\n";
                }
                std::cout << "---------------------------------------------\n";
            } else if (c=="detect" || c=="كشف"){
#if defined(_WIN32)
                std::cout << "Detecting approximate location via IP...\n";
                auto g = ip_geolocate();
                if (g){
                    double la, lo; std::string tzV, cityV, countryV; std::tie(la,lo,tzV,cityV,countryV) = *g;
                    // Try to snap to a known city from our database for better accuracy
                    fs::path exeDir = fs::path(argv[0]).parent_path();
                    fs::path dataDir = exeDir / "data";
#if !defined(_WIN32)
                    if (!fs::exists(dataDir / "cities.csv")) { fs::path sysData = "/usr/share/almuslim/data"; if (fs::exists(sysData / "cities.csv")) dataDir = sysData; }
#endif
                    auto cities = load_cities(dataDir);
                    auto lower = [](std::string s){ for(char &c: s) c=(char)std::tolower((unsigned char)c); return s; };
                    std::string q = cityV; if (!countryV.empty()) q += ", " + countryV;
                    std::string ql = lower(q);
                    // Synonym fixes (common transliterations)
                    if (ql.find("buraydah")!=std::string::npos) q = "Buraidah, Saudi Arabia";
                    if (ql.find("mecca")!=std::string::npos) q = "Makkah, Saudi Arabia";
                    if (ql.find("medina")!=std::string::npos) q = "Madinah, Saudi Arabia";
                    std::optional<City> snap;
                    if (!cities.empty()) snap = find_best_city_match(cities, q);
                    std::unordered_map<std::string,std::string> updates;
                    auto qstr = [](const std::string &s){ return '"' + s + '"'; };
                    if (snap){
                        const City &c0 = *snap;
                        std::cout << "Matched to database city: " << c0.name << ", " << c0.country << " (" << c0.lat << ", " << c0.lon << ") tz: " << c0.tz << "\n";
                        std::cout << "Use this match? [Y/n]: ";
                        std::string ans; std::getline(std::cin, ans); std::string al = lower(ans);
                        bool use = (ans.empty() || al=="y" || al=="yes");
                        if (use){
                            updates["city"] = qstr(c0.name + ", " + c0.country);
                            updates["latitude"] = std::to_string(c0.lat);
                            updates["longitude"] = std::to_string(c0.lon);
                            updates["timezone"] = qstr(c0.tz);
                        }
                    }
                    // If no snap or user chose not to use it, persist raw IP values
                    if (updates.empty()){
                        updates["latitude"] = std::to_string(la);
                        updates["longitude"] = std::to_string(lo);
                        if (!tzV.empty()) updates["timezone"] = qstr(tzV);
                        if (!cityV.empty()) {
                            std::string full = cityV; if (!countryV.empty()) full += ", " + countryV;
                            updates["city"] = qstr(full);
                        }
                        // Country fallback TZ if missing
                        if (tzV.empty() && lower(countryV)=="saudi arabia") updates["timezone"] = qstr("Asia/Riyadh");
                    }
                    write_or_update_config(config, updates);
                    std::cout << "Saved location to config.\n";
                    // Offer to set a custom display city string; if it matches our DB, also update coords/timezone
                    std::cout << "Enter a custom city name to display (or blank to keep): ";
                    std::string cname; std::getline(std::cin, cname);
                    if (!cname.empty()){
                        std::unordered_map<std::string,std::string> up2; up2["city"] = '"' + cname + '"';
                        // Attempt to match this custom name to our database for more accurate coordinates
                        fs::path exeDir2 = fs::path(argv[0]).parent_path();
                        fs::path dataDir2 = exeDir2 / "data";
#if !defined(_WIN32)
                        if (!fs::exists(dataDir2 / "cities.csv")) { fs::path sysData2 = "/usr/share/almuslim/data"; if (fs::exists(sysData2 / "cities.csv")) dataDir2 = sysData2; }
#endif
                        auto cities2 = load_cities(dataDir2);
                        auto m = find_best_city_match(cities2, cname);
                        if (m){
                            const City &cx = *m;
                            up2["city"] = '"' + (cx.name + ", " + cx.country) + '"';
                            up2["latitude"] = std::to_string(cx.lat);
                            up2["longitude"] = std::to_string(cx.lon);
                            up2["timezone"] = '"' + cx.tz + '"';
                            std::cout << "Matched and applied: " << cx.name << ", " << cx.country << " (" << cx.lat << ", " << cx.lon << ") tz: " << cx.tz << "\n";
                        }
                        write_or_update_config(config, up2);
                        std::cout << "Saved custom city.\n";
                    }
                    cfg = read_simple_kv(config);
                    render_main_view(argv[0], config, cfg);
                    continue;
                } else {
                    std::cout << "Could not detect location.\n";
                }
            } else if (c=="refresh" || c=="r" || c=="تحديث"){
                cfg = read_simple_kv(config);
                render_main_view(argv[0], config, cfg);
                continue;
#else
                std::cout << "Location detection currently supported on Windows only.\n";
#endif
            } else if (c.rfind("coords",0)==0 || c.rfind("إحداثيات",0)==0){
                // Usage: coords <lat> <lon> [timezone]
                std::istringstream iss(cmd);
                std::string kw; iss >> kw;
                std::string sLat, sLon, sTz; iss >> sLat >> sLon; std::getline(iss, sTz);
                auto trim = [](std::string s){ s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c){return !std::isspace(c);})); s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c){return !std::isspace(c);}).base(), s.end()); return s; };
                sTz = trim(sTz);
                if (sLat.empty() || sLon.empty()){
                    std::cout << "Usage: coords <lat> <lon> [timezone]  e.g., coords 26.325 43.975 Asia/Riyadh\n";
                } else {
                    try{
                        double la = std::stod(sLat); double lo = std::stod(sLon);
                        std::unordered_map<std::string,std::string> up;
                        up["latitude"] = std::to_string(la);
                        up["longitude"] = std::to_string(lo);
                        if (!sTz.empty()) up["timezone"] = '"' + sTz + '"';
                        write_or_update_config(config, up);
                        std::cout << "Coordinates saved.\n";
                        cfg = read_simple_kv(config);
                        render_main_view(argv[0], config, cfg);
                        continue;
                    } catch(...){ std::cout << "Invalid numbers.\n"; }
                }
            } else if (c.rfind("city ",0)==0 || c.rfind("مدينة ",0)==0){
                // Directly set city by name using DB matching
                std::string name = cmd.substr(cmd.find(' ')+1);
                auto trim = [](std::string s){ s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c){return !std::isspace(c);})); s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c){return !std::isspace(c);}).base(), s.end()); return s; };
                name = trim(name);
                if (name.empty()){
                    std::cout << "Usage: city <name>   e.g., city Buraidah\n";
                } else {
                    fs::path exeDir = fs::path(argv[0]).parent_path();
                    fs::path dataDir = exeDir / "data";
#if !defined(_WIN32)
                    if (!fs::exists(dataDir / "cities.csv")) { fs::path sysData = "/usr/share/almuslim/data"; if (fs::exists(sysData / "cities.csv")) dataDir = sysData; }
#endif
                    auto cities = load_cities(dataDir);
                    auto m = find_best_city_match(cities, name);
                    if (m){
                        const City &c0 = *m;
                        std::unordered_map<std::string,std::string> up;
                        auto q = [](const std::string &s){ return '"' + s + '"'; };
                        up["city"] = q(c0.name + ", " + c0.country);
                        up["latitude"] = std::to_string(c0.lat);
                        up["longitude"] = std::to_string(c0.lon);
                        up["timezone"] = q(c0.tz);
                        write_or_update_config(config, up);
                        std::cout << "City updated.\n";
                        cfg = read_simple_kv(config);
                        render_main_view(argv[0], config, cfg);
                        continue;
                    } else {
                        std::cout << "No matching city found in database.\n";
                    }
                }
            } else if (c=="quit" || c=="exit" || c=="خروج"){
                return 0;
            } else {
                std::cout << "Unknown command. Type 'help' for options.";
            }
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
