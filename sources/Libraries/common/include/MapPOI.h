#pragma once
// Shared, pointer-free contract. All world positions are centimetres.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <utility>

namespace map_poi {
constexpr std::uint8_t Version = 1;
constexpr std::size_t MaxMarkers = 512;
constexpr std::size_t MaxName = 48;
enum class Kind : std::uint8_t { Boss=1, MiniBoss=2, Chest=3, Portal=4 };
enum class State : std::uint8_t { Active=1, Inactive=2, Removed=3 };
inline bool Valid(Kind kind) { return kind >= Kind::Boss && kind <= Kind::Portal; }
inline bool Public(Kind kind) { return kind == Kind::Boss || kind == Kind::MiniBoss; }
struct Marker {
    std::uint64_t id=0;
    Kind kind=Kind::Boss;
    State state=State::Active;
    std::int32_t x=0, y=0;
    std::uint32_t templateId=0;
    std::string name;
    bool operator==(const Marker& b) const {
        return id==b.id && kind==b.kind && state==b.state && x==b.x && y==b.y &&
               templateId==b.templateId && name==b.name;
    }
};
struct Entry { Marker marker; bool known=false; std::uint32_t revision=0; };
class Registry {
public:
    bool Add(Marker m, bool discovered=false) {
        if (!m.id || !Valid(m.kind) || entries.count(m.id) || entries.size()>=MaxMarkers) return false;
        m.name.resize((std::min)(m.name.size(),MaxName));
        Entry e{m, Public(m.kind)||discovered, 0};
        if(e.known) e.revision=++revision;
        entries.emplace(m.id,e); return true;
    }
    bool Discover(std::uint64_t id) {
        auto i=entries.find(id);
        if(i==entries.end() || i->second.known || i->second.marker.state!=State::Active) return false;
        i->second.known=true; i->second.revision=++revision; return true;
    }
    bool Update(Marker m) {
        auto i=entries.find(m.id); if(i==entries.end()) return false;
        m.name.resize((std::min)(m.name.size(),MaxName));
        if(i->second.marker==m) return false;
        // Categories and template identities are immutable for a point.
        if(m.kind!=i->second.marker.kind || m.templateId!=i->second.marker.templateId) return false;
        i->second.marker=m; if(i->second.known) i->second.revision=++revision; return true;
    }
    std::vector<Marker> Changes(std::uint32_t after, bool full) const {
        std::vector<Marker> out;
        for(const auto& p:entries) if(p.second.known && (full || p.second.revision>after))
            if(!full || p.second.marker.state!=State::Removed) out.push_back(p.second.marker);
        return out;
    }
    const Entry* Find(std::uint64_t id) const {auto i=entries.find(id); return i==entries.end()?nullptr:&i->second;}
    std::uint32_t Revision() const {return revision;}
    std::size_t Size() const {return entries.size();}
    // Caller MUST change the synchronization epoch before dropping tombstones.
    bool CompactRemoved() {
        bool changed=false;
        for(auto i=entries.begin();i!=entries.end();) {
            if(i->second.marker.state==State::Removed){i=entries.erase(i);changed=true;}
            else ++i;
        }
        return changed;
    }
    void Clear() {entries.clear(); revision=0;}
private:
    std::map<std::uint64_t,Entry> entries;
    std::uint32_t revision=0;
};
struct Projection {
    double left=0,top=0,width=0,height=0,centerX=0,centerY=0,pixelsPerCm=0;
    bool Contains(double x,double y,double margin=0) const {
        return x>=left+margin && y>=top+margin && x<left+width-margin && y<top+height-margin;
    }
    void Screen(double x,double y,double& sx,double& sy) const {
        sx=left+width/2+(x-centerX)*pixelsPerCm;
        sy=top+height/2+(y-centerY)*pixelsPerCm;
    }
    bool World(double sx,double sy,double& x,double& y) const {
        if(!(pixelsPerCm>0) || !std::isfinite(pixelsPerCm)) return false;
        x=centerX+(sx-left-width/2)/pixelsPerCm;
        y=centerY+(sy-top-height/2)/pixelsPerCm; return true;
    }
};
inline bool Near(std::int32_t ax,std::int32_t ay,std::int32_t bx,std::int32_t by,std::uint32_t radius) {
    const double dx=double(ax)-bx,dy=double(ay)-by;
    return dx*dx+dy*dy<=double(radius)*radius;
}
// Deterministic spawn key, independent of pool/world handles and pointer width.
inline std::uint64_t SpawnId(const std::string& map, std::uint32_t entityType,
    std::uint32_t templateId,std::int32_t x,std::int32_t y,std::uint32_t slot=0) {
    std::uint64_t h=UINT64_C(14695981039346656037);
    auto byte=[&](std::uint8_t b){h=(h^b)*UINT64_C(1099511628211);};
    for(unsigned char c:map) byte(c);
    byte(0);
    for(auto v:{entityType,templateId,static_cast<std::uint32_t>(x),static_cast<std::uint32_t>(y),slot})
        for(int s=24;s>=0;s-=8) byte(static_cast<std::uint8_t>(v>>s));
    return h?h:1;
}
struct Message {
    std::string map;
    std::uint16_t copy=0;
    std::uint32_t context=0, revision=0;
    std::uint64_t epoch=0;
    bool full=true;
    std::vector<Marker> markers;
};
class Writer {
public:
    std::vector<std::uint8_t> bytes;
    void U(std::uint64_t n,int size) {for(int s=(size-1)*8;s>=0;s-=8) bytes.push_back(static_cast<std::uint8_t>(n>>s));}
    void Text(const std::string& s) {U(s.size(),1); bytes.insert(bytes.end(),s.begin(),s.end());}
};
class Reader {
public:
    Reader(const std::uint8_t* p,std::size_t n):p(p),n(n){}
    std::uint64_t U(int size) {
        if(size<0 || std::size_t(size)>n){ok=false; return 0;}
        std::uint64_t v=0; while(size--){v=(v<<8)|*p++;--n;} return v;
    }
    std::string Text(std::size_t max) {
        auto size=U(1); if(!ok || size>max || size>n){ok=false;return {};}
        std::string s(reinterpret_cast<const char*>(p),static_cast<std::size_t>(size));
        p+=size;n-=static_cast<std::size_t>(size);
        for(unsigned char c:s) if(c<32 || c==127) ok=false;
        return s;
    }
    bool End() const {return ok && n==0;}
    bool Good() const {return ok;}
private:
    const std::uint8_t* p; std::size_t n; bool ok=true;
};
inline std::vector<std::uint8_t> Encode(const Message& m) {
    if(m.map.empty() || m.map.size()>31 || m.markers.size()>MaxMarkers) return {};
    Writer w;w.U(Version,1);w.Text(m.map);w.U(m.copy,2);w.U(m.context,4);
    w.U(m.epoch,8);w.U(m.revision,4);w.U(m.full?1:0,1);w.U(m.markers.size(),2);
    for(const auto& p:m.markers) {
        if(!p.id || !Valid(p.kind) || p.name.size()>MaxName || p.x<0 || p.y<0 ||
            p.state<State::Active || p.state>State::Removed) return {};
        w.U(p.id,8);w.U(static_cast<std::uint8_t>(p.kind),1);w.U(static_cast<std::uint8_t>(p.state),1);
        w.U(static_cast<std::uint32_t>(p.x),4);w.U(static_cast<std::uint32_t>(p.y),4);
        w.U(p.templateId,4);w.Text(p.name);
    }
    return w.bytes;
}
inline bool Decode(const std::uint8_t* data,std::size_t size,Message& output) {
    if(!data || size>65535) return false;
    Reader r(data,size);if(r.U(1)!=Version) return false;
    Message m;m.map=r.Text(31);m.copy=static_cast<std::uint16_t>(r.U(2));
    m.context=static_cast<std::uint32_t>(r.U(4));m.epoch=r.U(8);m.revision=static_cast<std::uint32_t>(r.U(4));
    auto full=r.U(1),count=r.U(2);if(full>1 || count>MaxMarkers || m.map.empty())return false;m.full=full!=0;
    for(std::size_t i=0;i<count && r.Good();++i) {
        Marker p;p.id=r.U(8);p.kind=static_cast<Kind>(r.U(1));p.state=static_cast<State>(r.U(1));
        p.x=static_cast<std::int32_t>(r.U(4));p.y=static_cast<std::int32_t>(r.U(4));
        p.templateId=static_cast<std::uint32_t>(r.U(4));p.name=r.Text(MaxName);
        if(!p.id || !Valid(p.kind) || p.state<State::Active || p.state>State::Removed || p.x<0 || p.y<0) return false;
        m.markers.push_back(p);
    }
    if(!r.End() || m.markers.size()!=count) return false;
    std::map<std::uint64_t,bool> ids; for(const auto& p:m.markers)if(!ids.emplace(p.id,true).second)return false;
    output=std::move(m);return true;
}
class ClientStore {
public:
    std::map<std::uint64_t,Marker> points;
    std::string map;
    std::uint32_t context=1, revision=0;
    std::uint64_t epoch=0;
    std::uint16_t copy=0;
    void Reset(const std::string& name) {map=name;points.clear();revision=0;epoch=0;copy=0;if(++context==0)++context;}
    bool Apply(const Message& m) {
        if(m.context!=context || m.map!=map || (!m.full && (m.epoch!=epoch || m.copy!=copy)) ||
           (m.epoch==epoch && m.revision<revision)) return false;
        auto next=m.full?std::map<std::uint64_t,Marker>{}:points;
        for(const auto& p:m.markers) {
            if(p.state==State::Removed)next.erase(p.id);else next[p.id]=p;
            if(next.size()>MaxMarkers)return false;
        }
        points.swap(next);epoch=m.epoch;copy=m.copy;revision=m.revision;return true;
    }
};
} // namespace map_poi
