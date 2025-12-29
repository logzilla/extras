#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <fstream>
#include "SyslogAgentSharedConstants.h"

namespace Syslog_agent {

/*  ------------------------------------------------------------------------
    Registry  –  minimal, header-only helper

    • main_key_      HKLM\Software\LogZilla\SyslogAgent          (KEY_READ)
    • channels_key_  …\Channels                                 (KEY_READ|ENUM)

    All writes open a short-lived handle with KEY_SET_VALUE (or create sub-key
    with KEY_WRITE | KEY_SET_VALUE).  No global write access is held.
    ------------------------------------------------------------------------ */
class Registry {
public:
    Registry() noexcept = default;
    ~Registry() noexcept { close(); }

    /* ─────────────── lifetime ─────────────── */
    void close() noexcept {
        if (channels_key_) { RegCloseKey(channels_key_); channels_key_ = nullptr; }
        if (main_key_)     { RegCloseKey(main_key_);     main_key_     = nullptr; }
    }

    void open() {                                          // production default
        close();
        RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\LogZilla\\SyslogAgent",
                      0, KEY_READ, &main_key_);
        RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\LogZilla\\SyslogAgent\\Channels",
                      0, KEY_READ | KEY_ENUMERATE_SUB_KEYS,
                      &channels_key_);                     // may fail → nullptr
    }

    void open(HKEY parent, const wchar_t* sub,
              REGSAM sam = KEY_READ | KEY_WRITE) {
        close();
        if (!parent || !sub || !*sub)
            throw std::invalid_argument("Registry::open bad args");

        // Open the main key
        if (RegOpenKeyExW(parent, sub, 0, sam, &main_key_) != ERROR_SUCCESS)
            throw std::runtime_error("Registry::open RegOpenKeyEx failed");

        // Attempt to open the 'Channels' subkey relative to the main key
        std::wstring channelsPath = std::wstring(sub) + L"\\Channels";
        // Use KEY_READ | KEY_ENUMERATE_SUB_KEYS for channels, same as default open()
        RegOpenKeyExW(parent, channelsPath.c_str(), 0, KEY_READ | KEY_ENUMERATE_SUB_KEYS, &channels_key_);
        // Note: channels_key_ might remain nullptr if it doesn't exist or fails to open,
        // which is acceptable and handled by readChannels().
    }
    void open(Registry& parent, const wchar_t* sub,
              REGSAM sam = KEY_READ | KEY_WRITE) {
        open(parent.handle(), sub, sam);
    }

    HKEY handle() const noexcept { return main_key_; }

    /* ─────────────── read helpers ─────────── */
    bool   readBool (const wchar_t* n, bool   d=false) const { DWORD v=0; return query(n,v)?v!=0:d; }
    char   readChar (const wchar_t* n, char   d=0)     const { DWORD v=0; return query(n,v)?char(v):d; }
    int    readInt  (const wchar_t* n, int    d=0)     const { DWORD v=0; return query(n,v)?int(v):d; }
    time_t readTime (const wchar_t* n, time_t d=0)     const { time_t v=0;return query(n,v)?v:d; }

    std::wstring readString(const wchar_t* n, const wchar_t* def=L"") const {
        if (!main_key_) {
            return def ? def : L"";
        }

        DWORD type = 0, sz = 0; // sz = size in bytes
        LSTATUS status = RegQueryValueExW(main_key_, n, nullptr, &type, nullptr, &sz);

        if (status == ERROR_FILE_NOT_FOUND) {
            return def ? def : L"";
        }
        if (status != ERROR_SUCCESS || type != REG_SZ || sz == 0 || (sz % sizeof(wchar_t)) != 0) {
             return def ? def : L"";
        }

        size_t buf_elements = sz / sizeof(wchar_t);
        std::vector<wchar_t> buf(buf_elements);
        DWORD bytes_to_read = sz;

        status = RegQueryValueExW(main_key_, n, nullptr, nullptr, // Type already checked
                                  reinterpret_cast<LPBYTE>(buf.data()), &bytes_to_read);

        if (status != ERROR_SUCCESS) {
            return def ? def : L"";
        }

        size_t chars = buf_elements - 1;
        std::wstring result = {buf.data(), chars};
        return result;
    }

    /* ─────────────── write helpers ────────── */
    void writeUint(const wchar_t* n, DWORD  v) const {
        if (!main_key_) return;
        RegSetValueExW(main_key_, n, 0, REG_DWORD, 
                       reinterpret_cast<const BYTE*>(&v), sizeof(DWORD));
    }
    void writeTime(const wchar_t* n, time_t v) const {
        if (!main_key_) return;
        DWORD value = static_cast<DWORD>(v);
        RegSetValueExW(main_key_, n, 0, REG_DWORD, 
                       reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
    }

    /* ─────────────── channels ─────────────── */
    std::vector<std::wstring> readChannels() const {
        std::vector<std::wstring> out;
        if (!channels_key_) return out;

        wchar_t name[256];
        for (DWORD i=0;;++i) {
            DWORD len=_countof(name);
            auto st=RegEnumKeyExW(channels_key_,i,name,&len,nullptr,nullptr,nullptr,nullptr);
            if (st==ERROR_NO_MORE_ITEMS) break;
            if (st!=ERROR_SUCCESS)       break;
            // Determine whether this channel is enabled by inspecting its subkey value.
            // Default to enabled (backward compatible) if the value is missing or unreadable.
            bool enabled = true;
            HKEY channel_key = nullptr;
            if (RegOpenKeyExW(channels_key_, name, 0, KEY_READ, &channel_key) == ERROR_SUCCESS) {
                DWORD enabled_val = 0;
                DWORD type = 0;
                DWORD sz = sizeof(enabled_val);
                if (RegQueryValueExW(channel_key,
                    SharedConstants::RegistryKey::CHANNEL_ENABLED,
                    nullptr, &type,
                    reinterpret_cast<LPBYTE>(&enabled_val), &sz) == ERROR_SUCCESS &&
                    type == REG_DWORD) {
                    enabled = (enabled_val != 0);
                }
                RegCloseKey(channel_key);
            }
            if (enabled) {
                out.emplace_back(name,len);
            }
        }
        return out;
    }

    /* ─────────────── bookmarks ────────────── */
    static std::wstring readBookmark(const wchar_t* channel) {
        if (!channel||!*channel) return L"";
        std::wstring path=L"SOFTWARE\\LogZilla\\SyslogAgent\\Channels\\"; path+=channel;

        HKEY h=nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,path.c_str(),0,KEY_READ,&h)!=ERROR_SUCCESS)
            return L"";

        DWORD type=0, sz=0;
        if (RegQueryValueExW(h,L"Bookmark",nullptr,&type,nullptr,&sz)!=ERROR_SUCCESS||type!=REG_SZ)
        { RegCloseKey(h); return L""; }

        std::vector<wchar_t> buf(sz/sizeof(wchar_t));
        RegQueryValueExW(h,L"Bookmark",nullptr,nullptr,
                         reinterpret_cast<LPBYTE>(buf.data()),&sz);
        RegCloseKey(h);
        size_t chars=sz/sizeof(wchar_t); if(chars&&buf[chars-1]==L'\0')--chars;
        return {buf.data(),chars};
    }

    static void writeBookmark(const wchar_t* channel,const wchar_t* bm,DWORD szBytes){
        if(!channel||!*channel||!bm||szBytes==0) return;
        std::wstring path=L"SOFTWARE\\LogZilla\\SyslogAgent\\Channels\\"; path+=channel;

        HKEY h=nullptr; DWORD disp=0;
        if(RegCreateKeyExW(HKEY_LOCAL_MACHINE,path.c_str(),0,nullptr,
                           REG_OPTION_NON_VOLATILE,
                           KEY_WRITE|KEY_SET_VALUE,nullptr,&h,&disp)!=ERROR_SUCCESS)
            return;
        RegSetValueExW(h,L"Bookmark",0,REG_SZ,
                       reinterpret_cast<const BYTE*>(bm),szBytes);
        RegCloseKey(h);
    }

    // Read the per-channel Enabled flag; defaults to true if missing/unreadable.
    static bool readChannelEnabled(const wchar_t* channel) {
        if(!channel||!*channel) return true;
        std::wstring path=L"SOFTWARE\\LogZilla\\SyslogAgent\\Channels\\"; path+=channel;

        HKEY h=nullptr;
        if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,path.c_str(),0,KEY_READ,&h)!=ERROR_SUCCESS)
            return true; // default enabled if key missing

        DWORD enabled_val = 1;
        DWORD type = 0;
        DWORD sz = sizeof(enabled_val);
        if (RegQueryValueExW(h,
            SharedConstants::RegistryKey::CHANNEL_ENABLED,
            nullptr, &type,
            reinterpret_cast<LPBYTE>(&enabled_val), &sz) != ERROR_SUCCESS ||
            type != REG_DWORD) {
            enabled_val = 1; // default enabled
        }
        RegCloseKey(h);
        return enabled_val != 0;
    }

    /* ─────────────── setup.txt (stub) ─────── */
    static void loadSetupFile() {}   // (already implemented earlier if desired)

private:
    /* helper for read helpers */
    template<typename T>
    bool query(const wchar_t* n, T& out) const {
        if(!main_key_) return false;
        DWORD sz=sizeof out;
        return RegQueryValueExW(main_key_,n,nullptr,nullptr,
               reinterpret_cast<LPBYTE>(&out),&sz)==ERROR_SUCCESS;
    }

    /* helper that acquires a *temporary* KEY_SET_VALUE handle */
    static void setValue(const wchar_t* subKey,const wchar_t* name,
                         DWORD type,const void* data,DWORD sz){
        HKEY h=nullptr;
        if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,subKey,0,KEY_SET_VALUE,&h)!=ERROR_SUCCESS)
            throw std::runtime_error("Registry::setValue open failed");
        RegSetValueExW(h,name,0,type,reinterpret_cast<const BYTE*>(data),sz);
        RegCloseKey(h);
    }

    /* handles */
    HKEY main_key_{nullptr};
    HKEY channels_key_{nullptr};
};

} // namespace Syslog_agent
