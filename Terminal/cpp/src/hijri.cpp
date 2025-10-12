#include "hijri.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>
#include <ctime>
#include <cstring>

namespace hijri {

struct MonthStart { int hy; int hm; int gy; int gm; int gd; };
static std::vector<MonthStart> g_starts;

static bool less_ms(const MonthStart& a, const MonthStart& b){
    if (a.gy != b.gy) return a.gy < b.gy;
    if (a.gm != b.gm) return a.gm < b.gm;
    return a.gd < b.gd;
}

bool load_umm_al_qura(const std::filesystem::path& dataDir){
    g_starts.clear();
    std::filesystem::path csv = dataDir / "hijri" / "umm_al_qura_month_starts.csv";
    std::ifstream in(csv);
    if (!in) return false;
    std::string line; bool header = true;
    while (std::getline(in, line)){
        if (header){ header=false; continue; }
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string hy,hm,gy,gm,gd;
        if (!std::getline(ss, hy, ',')) continue;
        std::getline(ss, hm, ',');
        std::getline(ss, gy, ',');
        std::getline(ss, gm, ',');
        std::getline(ss, gd, ',');
        MonthStart ms; ms.hy=0; ms.hm=0; ms.gy=0; ms.gm=0; ms.gd=0;
        try{
            ms.hy = std::stoi(hy); ms.hm = std::stoi(hm);
            ms.gy = std::stoi(gy); ms.gm = std::stoi(gm); ms.gd = std::stoi(gd);
        } catch(...){ continue; }
        g_starts.push_back(ms);
    }
    std::sort(g_starts.begin(), g_starts.end(), less_ms);
    return !g_starts.empty();
}

static int cmp_gdate(int y,int m,int d, const std::tm& t){
    if (y != (t.tm_year+1900)) return (y < (t.tm_year+1900)) ? -1 : 1;
    if (m != (t.tm_mon+1)) return (m < (t.tm_mon+1)) ? -1 : 1;
    if (d != t.tm_mday) return (d < t.tm_mday) ? -1 : 1;
    return 0;
}

std::optional<HijriDate> hijri_for_date(const std::tm& localDate){
    if (g_starts.empty()) return std::nullopt;
    const MonthStart* last = nullptr;
    for (size_t i=0;i<g_starts.size();++i){
        const MonthStart& s = g_starts[i];
        if (cmp_gdate(s.gy, s.gm, s.gd, localDate) <= 0) last = &s; else break;
    }
    if (!last) return std::nullopt;
    std::tm st; ::memset(&st, 0, sizeof(st)); st.tm_year = last->gy - 1900; st.tm_mon = last->gm - 1; st.tm_mday = last->gd; st.tm_isdst=-1;
    time_t t0 = mktime(&st);
    std::tm dt = localDate; dt.tm_isdst=-1; time_t t1 = mktime(&dt);
    if (t0 == (time_t)-1 || t1 == (time_t)-1) return std::nullopt;
    int delta = (int)((t1 - t0) / 86400);
    HijriDate h; h.year = last->hy; h.month = last->hm; h.day = delta + 1; if (h.day < 1) h.day = 1; if (h.day > 30) h.day = 30;
    return h;
}

const char* month_name_en(int m){
    static const char* N[12] = {"Muharram","Safar","Rabi' I","Rabi' II","Jumada I","Jumada II","Rajab","Sha'ban","Ramadan","Shawwal","Dhul-Qa'dah","Dhul-Hijjah"};
    if (m < 1 || m > 12) return ""; return N[m-1];
}
const char* month_name_ar(int m){
    static const char* N[12] = {"محرم","صفر","ربيع الأول","ربيع الآخر","جمادى الأولى","جمادى الآخرة","رجب","شعبان","رمضان","شوال","ذو القعدة","ذو الحجة"};
    if (m < 1 || m > 12) return ""; return N[m-1];
}

} // namespace hijri
