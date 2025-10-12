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

#if defined(_WIN32)
#include <windows.h>
#include <limits>
static bool should_pause_on_exit(){
    // If only this process is attached to the console, it's likely launched by double-click.
    DWORD dummy;
    DWORD n = GetConsoleProcessList(&dummy, 1);
    return n <= 1; // 0 on error, 1 means only us
}
static void pause_console(){
    std::cout << "\nPress Enter to exit...";
    std::cout.flush();
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}
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

// Math helpers
static inline double deg2rad(double d){ return d * M_PI / 180.0; }
static inline double rad2deg(double r){ return r * 180.0 / M_PI; }
static inline double clamp(double v, double lo, double hi){ return std::max(lo, std::min(hi, v)); }

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

int main(int argc, char** argv) {
    try {
        // Resolve config path
        fs::path config = platform::resolve_config_path();
        std::unordered_map<std::string, std::string> cfg;
        if (!config.empty() && fs::exists(config)) {
            cfg = read_simple_kv(config);
        }

        // Setup mode to select city interactively and write config
        if (argc > 1 && std::string(argv[1]) == "--setup"){
            fs::path exeDir = fs::path(argv[0]).parent_path();
            fs::path dataDir = exeDir / "data";
            auto cities = load_cities(dataDir);
            if (cities.empty()){
                std::cout << "No cities database found at " << (dataDir/"cities.csv").string() << "\n";
                return 1;
            }
            auto chosen = select_city_interactive(cities);
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

        std::cout << "Al-Muslim (C++)\n";
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
        bool use24h = true; { auto v = get("24h","true"); std::string s=v; std::transform(s.begin(),s.end(),s.begin(),::tolower); use24h = (s=="true"||s=="1"||s=="yes"); }

        std::cout << "City: " << city << "\n";
        std::cout << "Latitude: " << (latS.empty()?"(unset)":latS) << ", Longitude: " << (lonS.empty()?"(unset)":lonS) << "\n";
        std::cout << "Method: " << method << ", Madhab: " << madhab << "\n";
        std::cout << "High-latitude: " << hlr << "\n";

        double latitude=0.0, longitude=0.0;
        if (!latS.empty()) latitude = std::stod(latS);
        if (!lonS.empty()) longitude = std::stod(lonS);

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

    std::cout << "\n==============================\n";
    std::cout << " City     : " << city << "\n";
    std::cout << " Method   : " << method << " (" << madhab << ")" << "\n";
    std::cout << "------------------------------\n";
    std::cout << " Fajr     : " << fmt_time(pt.fajr, use24h) << "\n";
    std::cout << " Sunrise  : " << fmt_time(pt.sunrise, use24h) << "\n";
    std::cout << " Dhuhr    : " << fmt_time(pt.dhuhr, use24h) << "\n";
    std::cout << " Asr      : " << fmt_time(pt.asr, use24h) << "\n";
    std::cout << " Maghrib  : " << fmt_time(pt.maghrib, use24h) << "\n";
    std::cout << " Isha     : " << fmt_time(pt.isha, use24h) << "\n";
    std::cout << "==============================\n";

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
        char buf[32]; std::snprintf(buf, sizeof(buf), "%02d:%02d", std::max(0,h), std::max(0,m));
    std::cout << "\nNext (" << nextName << ") in: " << buf << "\n";
    std::cout << "Tip: run with --setup to pick your city interactively.\n";

    // Normal exit
#if defined(_WIN32)
    if (should_pause_on_exit()) pause_console();
#endif
    return 0;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
#if defined(_WIN32)
    if (should_pause_on_exit()) pause_console();
#endif
        return 1;
    }
}
