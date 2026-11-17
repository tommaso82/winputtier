#include "core.h"
#include <winhttp.h>
#include <shlobj.h>
#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <set>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")

namespace wp {
namespace {
std::wstring join(const std::wstring& a, const std::wstring& b) { return a + L"\\" + b; }
std::wstring utf8ToWide(const std::string& s, UINT cp = CP_UTF8) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(cp, cp == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0, s.data(), (int)s.size(), nullptr, 0);
    if (!n) return {};
    std::wstring out(n, L'\0');
    MultiByteToWideChar(cp, cp == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0, s.data(), (int)s.size(), &out[0], n);
    return out;
}
std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &out[0], n, nullptr, nullptr);
    return out;
}
std::wstring readText(const std::wstring& path) {
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"rb") || !f) return {};
    std::unique_ptr<FILE, decltype(&fclose)> holder(f, fclose);
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0 || len > 8*1024*1024) return {};
    rewind(f);
    std::string bytes((size_t)len, '\0');
    if (len && fread(&bytes[0], 1, bytes.size(), f) != bytes.size()) return {};
    if (bytes.size() >= 2 && (unsigned char)bytes[0] == 0xff && (unsigned char)bytes[1] == 0xfe) {
        std::wstring w;
        for (size_t i=2; i+1<bytes.size(); i+=2)
            w.push_back(wchar_t((unsigned char)bytes[i] | ((unsigned char)bytes[i+1] << 8)));
        return w;
    }
    if (bytes.size() >= 3 && bytes.substr(0,3) == "\xef\xbb\xbf") bytes.erase(0,3);
    auto w = utf8ToWide(bytes);
    return w.empty() && !bytes.empty() ? utf8ToWide(bytes, CP_ACP) : w;
}
bool writeTextUtf16(const std::wstring& path, const std::wstring& text) {
    FILE* f=nullptr;
    if (_wfopen_s(&f, path.c_str(), L"wb") || !f) return false;
    const unsigned char bom[2]={0xff,0xfe};
    bool ok=fwrite(bom,1,2,f)==2 && (text.empty() || fwrite(text.data(),sizeof(wchar_t),text.size(),f)==text.size());
    return fclose(f)==0 && ok;
}
bool writeTextUtf8(const std::wstring& path, const std::wstring& text) {
    FILE* f=nullptr;
    if (_wfopen_s(&f, path.c_str(), L"wb") || !f) return false;
    auto b=wideToUtf8(text);
    bool ok=b.empty() || fwrite(b.data(),1,b.size(),f)==b.size();
    return fclose(f)==0 && ok;
}
std::wstring errorCode(const wchar_t* label, LONG err) {
    return std::wstring(label) + L" (errore Windows " + std::to_wstring(err) + L")";
}
std::wstring colorText(const Color& c) {
    return std::to_wstring(c.r)+L","+std::to_wstring(c.g)+L","+std::to_wstring(c.b);
}
bool parseRgb(std::wstring s, Color& c) {
    int r=-1,g=-1,b=-1; wchar_t tail=0;
    if (swscanf_s(s.c_str(), L"%d,%d,%d %c", &r,&g,&b,&tail,1) != 3) return false;
    if (r<0||r>255||g<0||g>255||b<0||b>255) return false;
    c={r,g,b}; return true;
}
std::wstring regPath(const Session& s) { return L"Software\\SimonTatham\\PuTTY\\Sessions\\" + s.key; }
bool readExisting(HKEY key, int idx, bool& exists, std::wstring& val) {
    wchar_t name[32]; swprintf_s(name, L"Colour%d", idx);
    DWORD type=0, bytes=0;
    LONG e=RegQueryValueExW(key,name,nullptr,&type,nullptr,&bytes);
    if (e==ERROR_FILE_NOT_FOUND) { exists=false; val.clear(); return true; }
    if (e!=ERROR_SUCCESS || type!=REG_SZ || bytes > 4096 || bytes<sizeof(wchar_t)) return false;
    std::vector<wchar_t> buf(bytes/sizeof(wchar_t)+1,0);
    if (RegQueryValueExW(key,name,nullptr,&type,(LPBYTE)buf.data(),&bytes)!=ERROR_SUCCESS) return false;
    exists=true; val=buf.data(); return true;
}
bool writeValue(HKEY key, int idx, const std::wstring* val) {
    wchar_t name[32]; swprintf_s(name,L"Colour%d",idx);
    if (!val) {
        LONG e=RegDeleteValueW(key,name);
        return e==ERROR_SUCCESS || e==ERROR_FILE_NOT_FOUND;
    }
    return RegSetValueExW(key,name,0,REG_SZ,(const BYTE*)val->c_str(),(DWORD)((val->size()+1)*sizeof(wchar_t)))==ERROR_SUCCESS;
}
struct Existing { bool present; std::wstring value; };
bool scanValues(HKEY key, std::array<Existing,22>& old) {
    for(int i=0;i<22;++i)
        if(!readExisting(key,i,old[i].present,old[i].value)) return false;
    return true;
}
bool restoreValues(HKEY key, const std::array<Existing,22>& old) {
    bool ok=true;
    for(int i=0;i<22;++i)
        if(!writeValue(key,i,old[i].present?&old[i].value:nullptr)) ok=false;
    RegFlushKey(key);
    return ok;
}
std::wstring idForSession(const Session& s) {
    uint64_t h=14695981039346656037ull;
    for(wchar_t c:s.key) { h^=(uint16_t)c; h*=1099511628211ull; }
    wchar_t out[24]; swprintf_s(out,L"%016llx",(unsigned long long)h);
    return out;
}
std::wstring originalBackupPath(const Session& s) {
    return join(backupDir(), L"original-" + idForSession(s) + L".wpb");
}
std::wstring oldestLegacyBackupPath(const Session& s) {
    auto pattern = join(backupDir(), L"backup-" + idForSession(s) + L"-*.wpb");
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return {};
    std::wstring oldest;
    do {
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (oldest.empty() || data.cFileName < oldest) oldest = data.cFileName;
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return oldest.empty() ? L"" : join(backupDir(), oldest);
}
std::wstring backupText(const Session& s, const std::array<Existing,22>& old) {
    std::wstring out=L"WinPuttierBackup1\n"+s.key+L"\n";
    for(int i=0;i<22;++i) out+=std::to_wstring(i)+L"\t"+(old[i].present?old[i].value:L"-")+L"\n";
    return out;
}
bool loadBackup(const std::wstring& filename, const Session& s, std::array<Existing,22>& out) {
    std::wistringstream in(readText(filename));
    std::wstring line;
    if(!std::getline(in,line)||line!=L"WinPuttierBackup1") return false;
    if(!std::getline(in,line)||line!=s.key) return false;
    for(int i=0;i<22;++i) {
        if(!std::getline(in,line)) return false;
        std::wstring prefix=std::to_wstring(i)+L"\t";
        if(line.compare(0,prefix.size(),prefix)!=0) return false;
        out[i].value=line.substr(prefix.size());
        out[i].present=out[i].value!=L"-";
    }
    return true;
}
bool httpGet(const std::wstring& url, std::string& out, std::wstring& error, DWORD max=6*1024*1024) {
    URL_COMPONENTS p{}; p.dwStructSize=sizeof(p);
    wchar_t host[256]={}, path[4096]={}, extra[4096]={};
    p.lpszHostName=host; p.dwHostNameLength=_countof(host);
    p.lpszUrlPath=path; p.dwUrlPathLength=_countof(path);
    p.lpszExtraInfo=extra; p.dwExtraInfoLength=_countof(extra);
    if(!WinHttpCrackUrl(url.c_str(),0,0,&p) || p.nScheme!=INTERNET_SCHEME_HTTPS) { error=L"URL HTTPS non valido"; return false; }
    using H=std::unique_ptr<void,decltype(&WinHttpCloseHandle)>;
    H h(WinHttpOpen(L"WinPuttier/0.1 (Windows)",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0),WinHttpCloseHandle);
    if(!h){error=L"WinHTTP non disponibile";return false;}
    DWORD timeout=20000;
    WinHttpSetTimeouts(h.get(),10000,10000,timeout,timeout);
    H conn(WinHttpConnect(h.get(),host,p.nPort,0),WinHttpCloseHandle);
    if(!conn){error=L"Impossibile connettersi a GitHub";return false;}
    std::wstring resource=path; resource+=extra;
    H req(WinHttpOpenRequest(conn.get(),L"GET",resource.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE),WinHttpCloseHandle);
    if(!req){error=L"Richiesta HTTP fallita";return false;}
    DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(req.get(), WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy));
    const wchar_t* headers=L"Accept: application/vnd.github+json\r\n";
    if(!WinHttpSendRequest(req.get(),headers,(DWORD)-1,WINHTTP_NO_REQUEST_DATA,0,0,0) || !WinHttpReceiveResponse(req.get(),nullptr)) {
        error=L"Errore connessione HTTPS ("+std::to_wstring(GetLastError())+L")";return false;
    }
    DWORD status=0, sz=sizeof(status);
    if(!WinHttpQueryHeaders(req.get(),WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&sz,WINHTTP_NO_HEADER_INDEX) || status!=200) {
        error=L"GitHub HTTP "+std::to_wstring(status)+L" (eventuale limite API)";return false;
    }
    out.clear();
    for(;;) {
        BYTE buf[8192]; DWORD got=0;
        if(!WinHttpReadData(req.get(),buf,sizeof(buf),&got)){error=L"Errore lettura HTTPS";return false;}
        if(!got) break;
        if(out.size()+got>max){error=L"Risposta troppo grande";return false;}
        out.append((const char*)buf,got);
    }
    return true;
}
std::string jsonField(const std::string& object, const std::string& name) {
    auto pos=object.find("\""+name+"\"");
    if(pos==std::string::npos) return {};
    pos=object.find(':',pos+name.size()+2);
    if(pos==std::string::npos) return {};
    pos=object.find('"',pos+1);
    if(pos==std::string::npos) return {};
    std::string v;
    for(++pos;pos<object.size();++pos) {
        char c=object[pos];
        if(c=='"') break;
        if(c=='\\' && pos+1<object.size()) {
            char n=object[++pos];
            if(n=='"'||n=='\\'||n=='/') v.push_back(n);
            else if(n=='n') v.push_back('\n');
            else if(n=='t') v.push_back('\t');
            else if(n=='u' && pos+4<object.size()) {
                unsigned val=0;
                bool ok=true;
                for(int i=0;i<4;++i) {
                    char z=object[pos+1+i];
                    if(!isxdigit((unsigned char)z)){ok=false;break;}
                    val=val*16+(z>='0'&&z<='9'?z-'0':(tolower((unsigned char)z)-'a'+10));
                }
                if(ok) {pos+=4; auto u=wideToUtf8(std::wstring(1,(wchar_t)val)); v+=u;}
            }
        } else v.push_back(c);
    }
    return v;
}
std::vector<Theme> parseContents(const std::string& body, const std::wstring& source) {
    std::vector<Theme> result;
    size_t pos=0;
    while((pos=body.find('{',pos))!=std::string::npos) {
        size_t begin=pos; int depth=0; bool quoted=false,escape=false;
        for(;pos<body.size();++pos) {
            char c=body[pos];
            if(quoted) { if(escape)escape=false; else if(c=='\\')escape=true; else if(c=='"')quoted=false; }
            else {if(c=='"')quoted=true;else if(c=='{')++depth;else if(c=='}' && --depth==0){++pos;break;}}
        }
        if(depth!=0)break;
        const auto obj=body.substr(begin,pos-begin);
        auto name=jsonField(obj,"name"), url=jsonField(obj,"download_url");
        if(name.size()>4 && name.substr(name.size()-4)==".reg" && url.rfind("https://",0)==0) {
            Theme t;
            t.name=utf8ToWide(name.substr(0,name.size()-4));
            t.source=source; t.remoteUrl=utf8ToWide(url);t.available=false;
            if(!t.name.empty())result.push_back(std::move(t));
        }
    }
    return result;
}
std::wstring trim(std::wstring s) {
    while(!s.empty() && iswspace(s.back()))s.pop_back();
    size_t first=0;while(first<s.size()&&iswspace(s[first]))++first;
    return s.substr(first);
}
std::wstring folderThemePath(const std::wstring& folder, const std::wstring& source, const std::wstring& name) {
    return join(folder,safeFileName(source+L"__"+name)+L".reg");
}
} // namespace

const Palette& defaultPalette() {
    static const Palette p = {{ {187,187,187},{255,255,255},{0,0,0},{85,85,85},{0,0,0},{0,255,0},
        {0,0,0},{85,85,85},{187,0,0},{255,85,85},{0,187,0},{85,255,85},
        {187,187,0},{255,255,85},{0,0,187},{85,85,255},{187,0,187},{255,85,255},
        {0,187,187},{85,255,255},{187,187,187},{255,255,255} }};
    return p;
}
std::wstring dataDir() {
    PWSTR p=nullptr;std::wstring s;
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&p)) && p) {s=p;CoTaskMemFree(p);}
    if(s.empty())s=L".";
    return join(s,L"WinPuttier");
}
std::wstring customDir(){return join(dataDir(),L"themes");}
std::wstring cacheDir(){return join(dataDir(),L"cache");}
std::wstring backupDir(){return join(dataDir(),L"backups");}
bool ensureDirs() {
    auto mk=[](const std::wstring& x){return CreateDirectoryW(x.c_str(),nullptr)||GetLastError()==ERROR_ALREADY_EXISTS;};
    return mk(dataDir())&&mk(customDir())&&mk(cacheDir())&&mk(backupDir());
}
std::wstring safeFileName(std::wstring s) {
    for(auto& c:s) if(c<32||wcschr(L"<>:\"/\\|?*",c))c=L'_';
    if(s.size()>110)s.resize(110);
    while(!s.empty()&&(s.back()==L'.'||s.back()==L' '))s.pop_back();
    return s.empty()?L"theme":s;
}
std::vector<Session> sessions() {
    std::vector<Session> result;
    HKEY root=nullptr;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\SimonTatham\\PuTTY\\Sessions",0,KEY_ENUMERATE_SUB_KEYS,&root)!=ERROR_SUCCESS)return result;
    for(DWORD i=0;;++i) {
        wchar_t buf[512];DWORD len=_countof(buf);
        LONG e=RegEnumKeyExW(root,i,buf,&len,nullptr,nullptr,nullptr,nullptr);
        if(e==ERROR_NO_MORE_ITEMS)break;
        if(e!=ERROR_SUCCESS)continue;
        Session s; s.key=buf; s.label=s.key;
        size_t pos=0;while((pos=s.label.find(L"%20",pos))!=std::wstring::npos){s.label.replace(pos,3,L" ");++pos;}
        result.push_back(std::move(s));
    }
    RegCloseKey(root);
    std::sort(result.begin(),result.end(),[](const Session& a,const Session& b){return _wcsicmp(a.label.c_str(),b.label.c_str())<0;});
    return result;
}
bool parseReg(const std::wstring& contents, Theme& theme) {
    std::wistringstream in(contents);std::wstring line;
    Palette colors{};std::array<bool,22> seen{};
    while(std::getline(in,line)) {
        line=trim(line);
        if(line.rfind(L"\"Colour",0)!=0)continue;
        size_t i=7,begin=i;while(i<line.size()&&iswdigit(line[i]))++i;
        if(i==begin||i+3>line.size()||line.compare(i,3,L"\"=\"")!=0)continue;
        int index=-1;
        try{index=std::stoi(line.substr(begin,i-begin));}catch(...){return false;}
        if(index<0||index>=22||seen[index])return false;
        size_t start=i+3,end=line.find(L'"',start);
        if(end==std::wstring::npos)return false;
        Color c;
        if(!parseRgb(line.substr(start,end-start),c))return false;
        colors[index]=c;seen[index]=true;
    }
    if(!std::all_of(seen.begin(),seen.end(),[](bool v){return v;}))return false;
    theme.colors=colors;
    return true;
}
bool readRegFile(const std::wstring& path, Theme& theme) {return parseReg(readText(path),theme);}
std::wstring exportReg(const Theme& theme, const std::wstring& sessionKey) {
    std::wstring out=L"Windows Registry Editor Version 5.00\r\n\r\n[HKEY_CURRENT_USER\\Software\\SimonTatham\\PuTTY\\Sessions\\"+sessionKey+L"]\r\n";
    for(int i=0;i<22;++i)out+=L"\"Colour"+std::to_wstring(i)+L"\"=\""+colorText(theme.colors[i])+L"\"\r\n";
    return out;
}
bool saveCustomTheme(const Theme& theme, const std::wstring& path, std::wstring& error) {
    if(!writeTextUtf16(path,exportReg(theme,L"WinPuttier%20Theme"))) {
        error=L"Impossibile salvare il file .reg";return false;
    }
    return true;
}
std::vector<Theme> localThemes() {
    std::vector<Theme> out;
    out.push_back({L"Default (PuTTY)",L"Integrato",L"",defaultPalette(),true});
    for(const auto& source : {std::pair<std::wstring,std::wstring>{customDir(),L"Personali"},{cacheDir(),L"Scaricato"}}) {
        WIN32_FIND_DATAW d{};
        auto spec=join(source.first,L"*.reg");
        HANDLE h=FindFirstFileW(spec.c_str(),&d);
        if(h==INVALID_HANDLE_VALUE)continue;
        do {
            if(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;
            Theme theme;theme.name=d.cFileName;
            if(theme.name.size()>4)theme.name.resize(theme.name.size()-4);
            theme.source=source.second;
            if (source.second == L"Scaricato") {
                const size_t delimiter = theme.name.find(L"__");
                if (delimiter != std::wstring::npos) {
                    theme.source = theme.name.substr(0, delimiter);
                    theme.name.erase(0, delimiter + 2);
                }
            }
            if(readRegFile(join(source.first,d.cFileName),theme))out.push_back(std::move(theme));
        }while(FindNextFileW(h,&d));
        FindClose(h);
    }
    return out;
}
bool importTheme(const std::wstring& path, std::wstring& message) {
    Theme t;
    if(!readRegFile(path,t)){message=L"Il file non contiene tutti i 22 colori validi.";return false;}
    auto slash=path.find_last_of(L"\\/");
    std::wstring name=(slash==std::wstring::npos?path:path.substr(slash+1));
    if(name.size()>4)name.resize(name.size()-4);
    auto out=join(customDir(),safeFileName(name)+L".reg");
    if(GetFileAttributesW(out.c_str())!=INVALID_FILE_ATTRIBUTES) {
        message=L"Esiste gia' un tema con questo nome. Rinomina il file prima di importarlo.";return false;
    }
    if(!saveCustomTheme(t,out,message))return false;
    message=L"Tema importato: "+name;return true;
}
std::vector<Theme> remoteCatalog() {
    std::vector<Theme> out;
    std::wistringstream in(readText(join(dataDir(),L"catalog.tsv")));
    std::wstring line;
    while(std::getline(in,line)) {
        auto a=line.find(L'\t'),b=a==std::wstring::npos?a:line.find(L'\t',a+1);
        if(a==std::wstring::npos||b==std::wstring::npos)continue;
        Theme t;t.source=line.substr(0,a);t.name=line.substr(a+1,b-a-1);t.remoteUrl=trim(line.substr(b+1));t.available=false;
        if(t.remoteUrl.rfind(L"https://",0)==0)out.push_back(std::move(t));
    }
    return out;
}
std::wstring sourceConfigText() {
    const std::wstring path=join(dataDir(),L"sources.ini");
    if(GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES) return readText(path);
    return
       L"; WinPuttier - raccolte temi (modificabili)\r\n"
       L"; Formato: Nome = URL GitHub API della cartella oppure URL HTTPS del singolo file .reg\r\n"
       L"[Sources]\r\n"
       L"iTerm2-Color-Schemes = https://api.github.com/repos/mbadolato/iTerm2-Color-Schemes/contents/putty\r\n"
       L"Pretty PuTTY = https://api.github.com/repos/jacek0x/pretty-putty/contents\r\n"
       L"Dracula PuTTY = https://raw.githubusercontent.com/teeli/dracula-putty/master/dracula.reg\r\n"
       L"AlexAkulov = https://api.github.com/repos/AlexAkulov/putty-color-themes/contents\r\n";
}
namespace {
bool checkSourceUrl(const std::wstring& url) {
    if(url.size()>1800) return false;
    const std::wstring api=L"https://api.github.com/repos/";
    const std::wstring raw=L"https://raw.githubusercontent.com/";
    if(url.compare(0,api.size(),api)==0) {
        const auto pos=url.find(L"/contents",api.size());
        return pos!=std::wstring::npos && pos>api.size()+2 && url.find(L'?',pos)==std::wstring::npos;
    }
    if(url.compare(0,raw.size(),raw)==0) {
        const auto suffix=url.rfind(L".reg");
        return suffix==url.size()-4 && suffix>raw.size()+3;
    }
    return false;
}
bool parseSources(const std::wstring& input,std::vector<ThemeSource>& out,std::wstring& message) {
    out.clear();std::wistringstream stream(input);std::wstring line;
    std::set<std::wstring> names;
    bool inSection=false;
    int lineNumber=0;
    while(std::getline(stream,line)) {
        ++lineNumber;
        line=trim(line);
        if(line.empty()||line[0]==L';'||line[0]==L'#')continue;
        if(line==L"[Sources]") {inSection=true;continue;}
        if(line[0]==L'['){inSection=false;continue;}
        if(!inSection)continue;
        auto equal=line.find(L'=');
        if(equal==std::wstring::npos){message=L"Fonte non valida alla riga "+std::to_wstring(lineNumber);return false;}
        ThemeSource src{trim(line.substr(0,equal)),trim(line.substr(equal+1))};
        if(src.name.empty()||src.name.size()>70||src.name.find_first_of(L"\\/:*?\"<>|")!=std::wstring::npos ||
           !checkSourceUrl(src.url)) {
            message=L"Nome o URL non valido alla riga "+std::to_wstring(lineNumber);return false;
        }
        auto unique=src.name;
        std::transform(unique.begin(),unique.end(),unique.begin(),towlower);
        if(!names.insert(unique).second){message=L"Nome fonte duplicato: "+src.name;return false;}
        out.push_back(std::move(src));
        if(out.size()>30){message=L"Limite di 30 fonti";return false;}
    }
    if(!inSection && out.empty()){message=L"Manca la sezione [Sources]";return false;}
    return true;
}
} // namespace
std::vector<ThemeSource> configuredSources() {
    std::vector<ThemeSource> items;std::wstring error;
    parseSources(sourceConfigText(),items,error);
    return items;
}
bool updateSourceConfig(const std::wstring& input,std::wstring& message) {
    std::vector<ThemeSource> checked;
    if(!parseSources(input,checked,message))return false;
    if(!ensureDirs()){message=L"Impossibile creare la cartella di configurazione.";return false;}
    const auto temp=join(dataDir(),L"sources.ini.tmp"),dst=join(dataDir(),L"sources.ini");
    if(!writeTextUtf8(temp,input)){message=L"Impossibile scrivere le fonti.";return false;}
    if(!MoveFileExW(temp.c_str(),dst.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        DeleteFileW(temp.c_str());message=L"Impossibile salvare sources.ini";return false;
    }
    message=L"Fonti salvate: "+std::to_wstring(checked.size());
    return true;
}
bool refreshCatalog(std::wstring& message) {
    auto sources=configuredSources();
    std::vector<Theme> all;
    std::wstring errors;
    for(const auto& src:sources) {
        if(src.url.rfind(L"https://raw.githubusercontent.com/",0)==0) {
            auto begin=src.url.find_last_of(L'/');
            std::wstring name=src.url.substr(begin+1);
            if(name.size()>4)name.resize(name.size()-4);
            all.push_back({name,src.name,src.url,{},false});
            continue;
        }
        std::string body;std::wstring error;
        if(!httpGet(src.url,body,error,6*1024*1024)) {
            errors+=src.name+L": "+error+L"; ";continue;
        }
        auto list=parseContents(body,src.name);
        all.insert(all.end(),list.begin(),list.end());
    }
    if(all.empty()) {message=L"Nessuna fonte disponibile: "+errors;return false;}
    std::wstring file;
    for(auto& t:all) {
        for(auto& c:t.name)if(c==L'\t'||c==L'\r'||c==L'\n')c=L' ';
        file+=t.source+L"\t"+t.name+L"\t"+t.remoteUrl+L"\n";
    }
    auto temp=join(dataDir(),L"catalog.tmp");
    if(!writeTextUtf8(temp,file)){message=L"Errore scrittura catalogo";return false;}
    auto target=join(dataDir(),L"catalog.tsv");
    if(!MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        DeleteFileW(temp.c_str());message=L"Impossibile aggiornare il catalogo";return false;
    }
    message=L"Catalogo aggiornato: "+std::to_wstring(all.size())+L" temi.";
    if(!errors.empty())message+=L" Alcune fonti non disponibili: "+errors;
    return true;
}

bool downloadTheme(Theme& item, std::wstring& error) {
    if(item.remoteUrl.empty()){error=L"Il tema non ha un URL";return false;}
    std::string bytes;
    if(!httpGet(item.remoteUrl,bytes,error,128*1024))return false;
    Theme t=item;
    auto wide=utf8ToWide(bytes);
    if(wide.empty()&&!bytes.empty())wide=utf8ToWide(bytes,CP_ACP);
    if(!parseReg(wide,t)){error=L"La fonte non contiene una palette PuTTY valida";return false;}
    auto path=folderThemePath(cacheDir(),item.source,item.name);
    if(!saveCustomTheme(t,path,error))return false;
    item.colors=t.colors;item.available=true;
    return true;
}
// Create-once protects the original snapshot from subsequent theme changes.
// Never replace an existing file; the caller verifies its session identity and format.
bool themeFromSession(const Session& session, Theme& theme, std::wstring& error) {
    HKEY key=nullptr;
    LONG e=RegOpenKeyExW(HKEY_CURRENT_USER,regPath(session).c_str(),0,KEY_QUERY_VALUE,&key);
    if(e!=ERROR_SUCCESS){error=errorCode(L"Sessione non accessibile",e);return false;}
    std::array<Existing,22> existing{};
    bool scanned=scanValues(key,existing);
    RegCloseKey(key);
    if(!scanned){error=L"Impossibile leggere i colori della sessione.";return false;}
    theme={};
    theme.name=session.label+L" - colori attuali";
    theme.source=L"Personali";
    theme.available=true;
    theme.colors=defaultPalette();
    for(int i=0;i<22;++i) {
        if(existing[i].present && !parseRgb(existing[i].value,theme.colors[i])) {
            error=L"Valore RGB non valido nella sessione: Colour"+std::to_wstring(i);
            return false;
        }
    }
    return true;
}
bool preserveOriginalBackupFile(const std::wstring& path, const std::wstring& snapshot, std::wstring& message) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS) return true;
        message = L"Impossibile creare il backup originale (errore " + std::to_wstring(error) + L")";
        return false;
    }
    std::string bytes = wideToUtf8(snapshot);
    DWORD count = 0;
    bool ok = WriteFile(file, bytes.data(), (DWORD)bytes.size(), &count, nullptr) != FALSE &&
              count == bytes.size() && FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    if (!ok) {
        DeleteFileW(path.c_str());
        message = L"Salvataggio del backup originale fallito; nessuna modifica effettuata.";
    }
    return ok;
}
bool applyTheme(const Session& session,const Theme& theme,std::wstring& message) {
    HKEY key=nullptr;
    LONG e=RegOpenKeyExW(HKEY_CURRENT_USER,regPath(session).c_str(),0,KEY_QUERY_VALUE|KEY_SET_VALUE,&key);
    if(e!=ERROR_SUCCESS){message=errorCode(L"Sessione non accessibile",e);return false;}
    std::array<Existing,22> old{};
    if(!scanValues(key,old)){RegCloseKey(key);message=L"Impossibile leggere i colori correnti; nessuna modifica.";return false;}
    const auto path=originalBackupPath(session);
    std::array<Existing,22> original{};
    if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES) {
        // For users who ran an older preview build, the oldest historical snapshot
        // is the original; do not mistake the already modified colors for it.
        const auto legacy=oldestLegacyBackupPath(session);
        if(!legacy.empty()) {
            if(!loadBackup(legacy,session,original)){
                RegCloseKey(key);message=L"Backup precedente non valido; nessuna modifica.";return false;
            }
        } else {
            original=old;
        }
        if(!preserveOriginalBackupFile(path,backupText(session,original),message)){
            RegCloseKey(key);return false;
        }
    }
    if(!loadBackup(path,session,original)){
        RegCloseKey(key);
        message=L"Il backup originale esistente non e' valido. Nessuna modifica effettuata.";
        return false;
    }
    bool ok=true;
    for(int i=0;i<22;++i) {
        auto color=colorText(theme.colors[i]);
        if(!writeValue(key,i,&color)){ok=false;break;}
    }
    if(!ok) {
        bool undone=restoreValues(key,old);
        message=undone?L"Scrittura fallita; colori precedenti ripristinati.":
            L"ERRORE: ripristino parziale. Backup originale: "+path;
    } else {
        RegFlushKey(key);
        message=L"Tema applicato a "+session.label+L". Il backup originale e' conservato.";
    }
    RegCloseKey(key);
    return ok;
}
bool restoreLastBackup(const Session& session, std::wstring& message) {
    const auto path=originalBackupPath(session);
    std::array<Existing,22> original{};
    if(!loadBackup(path,session,original)){
        message=L"Backup originale assente o non valido per questa sessione.";return false;
    }
    HKEY key=nullptr;
    LONG e=RegOpenKeyExW(HKEY_CURRENT_USER,regPath(session).c_str(),0,KEY_QUERY_VALUE|KEY_SET_VALUE,&key);
    if(e!=ERROR_SUCCESS){message=errorCode(L"Sessione non accessibile",e);return false;}
    std::array<Existing,22> before{};
    if(!scanValues(key,before)){RegCloseKey(key);message=L"Impossibile leggere i colori attuali.";return false;}
    bool ok=restoreValues(key,original);
    if(!ok) {
        const bool reversed=restoreValues(key,before);
        message=reversed?L"Ripristino fallito; configurazione precedente conservata.":
                         L"ERRORE: ripristino parziale. Backup originale: "+path;
    } else message=L"Ripristinati i colori ORIGINALI di "+session.label+
                    L". Il backup originale rimane disponibile.";
    RegCloseKey(key);
    return ok;
}

} // namespace wp
