#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <detours/detours.h>
#include <wil/win32_helpers.h>
#include <wil/win32_result_macros.h>
#include "common.hpp"
#include "dumper.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <span>
#include <vector>

namespace typetree_ripper::layout
{
    std::string QueryEngineProductVersion();
    uintptr_t FindTransferVftable();
}

#pragma comment(lib, "Version.lib")
#undef WIN32_LEAN_AND_MEAN

namespace
{
    HMODULE GetUnityModule()
    {
        for (const auto &module : {
            L"UnityPlayer.dll",
            L"Unity.dll",
            L"Unity.exe",
            L"TuanjiePlayer.dll",
            L"Tuanjie.dll",
            L"Tuanjie.exe"
            })
        {
            const auto hModule = GetModuleHandleW(module);
            if (hModule != nullptr)
                return hModule;
        }

        FAIL_FAST_WIN32(ERROR_NOT_FOUND);
    }
}

template<Revision R, Variant V>
class WindowsDumper
{
public:
    std::span<ExecutableSection> GetExecutableSections()
    {
        if (CachedSections.empty())
        {
            auto pDosHeader = reinterpret_cast<IMAGE_DOS_HEADER *>(GetUnityModule());
            auto pNtHeaders = reinterpret_cast<IMAGE_NT_HEADERS *>(reinterpret_cast<char *>(pDosHeader) + pDosHeader->e_lfanew);

            for (WORD iSection = 0; iSection < pNtHeaders->FileHeader.NumberOfSections; iSection++)
            {
                auto pSection = IMAGE_FIRST_SECTION(pNtHeaders) + iSection;
                auto pContent = reinterpret_cast<char *>(pDosHeader) + pSection->VirtualAddress;

                // Ignore all discardable sections
                if ((pSection->Characteristics & IMAGE_SCN_MEM_DISCARDABLE) != 0)
                    continue;

                auto protection = 0;
                if ((pSection->Characteristics & IMAGE_SCN_MEM_READ) != 0)
                    protection |= ExecutableSection::kSectionProtectionRead;

                if ((pSection->Characteristics & IMAGE_SCN_MEM_WRITE) != 0)
                    protection |= ExecutableSection::kSectionProtectionWrite;

                if ((pSection->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0)
                    protection |= ExecutableSection::kSectionProtectionExecute;

                const auto data = std::span(pContent, pSection->Misc.VirtualSize);
                CachedSections.emplace_back(data, protection);
            }
        }

        return CachedSections;
    }

    static std::ofstream CreateOutputFile(char const *filename)
    {
        // Create the output file in the current directory.
        // NOTE: Should this be made configurable? i.e. through an environment variable?
        return std::ofstream(std::filesystem::current_path() / filename, std::ios::out | std::ios::binary);
    }

    // Progress goes to the debugger (and therefore to the editor log).
    static void DebugLog(char const *message)
    {
        OutputDebugStringA(message);
        OutputDebugStringA("\n");
    }
private:
    std::vector<ExecutableSection> CachedSections;
};

//
// The dumper has two primary requirements:
//  1. It must run on the Unity main thread.
//  2. The engine needs to be initialized (so that object creation may succeed).
// To do this, we hook SetWindowLongPtrA (this is called right after engine init)
// to kick off the dumper and return a fail
// to cause the engine to quit afterwards.
//

namespace
{
    constexpr auto kForceRevisionEnvironmentVariable = L"TYPETREERIPPER_FORCE_REVISION";
    constexpr auto kForceVariantEnvironmentVariable = L"TYPETREERIPPER_FORCE_VARIANT";

    std::optional<RevisionVersion> TuanjieVersionFromProductVersion(std::string const &productVersion)
    {
        if (productVersion.empty())
            return std::nullopt;

        try
        {
            const auto version = ParseVersionString<std::string, std::istringstream, '.'>(productVersion);

            if (std::get<3>(version) == 0)
                return std::nullopt;

            return version;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    std::optional<Revision> GetUnityRevisionFromExecutable()
    {
        const auto module = GetUnityModule();
        const auto unityModuleFilepath = wil::GetModuleFileNameW(module);

        if (const auto size = GetFileVersionInfoSizeW(unityModuleFilepath.get(), nullptr);
            size > 0)
        {
            const auto versionData = std::make_unique<char[]>(size);
            if (GetFileVersionInfoW(unityModuleFilepath.get(), NULL, size, versionData.get()))
            {
                void *versionBuffer = nullptr;
                auto versionBufferSize = 0u;

                if (VerQueryValueW(versionData.get(), L"\\", &versionBuffer, &versionBufferSize)
                    && versionBuffer)
                {
                    if (const auto versionInfo = static_cast<VS_FIXEDFILEINFO *>(versionBuffer);
                        versionInfo->dwSignature == VS_FFI_SIGNATURE)
                    {
                        const auto major = HIWORD(versionInfo->dwFileVersionMS);
                        const auto minor = LOWORD(versionInfo->dwFileVersionMS);
                        const auto patch = HIWORD(versionInfo->dwFileVersionLS);

                        if (const auto tuanjieVersion = TuanjieVersionFromProductVersion(
                                typetree_ripper::layout::QueryEngineProductVersion());
                            tuanjieVersion.has_value())
                        {
                            const auto &[tMajor, tMinor, tPatch, tBuild] = tuanjieVersion.value();
                            return VersionToRevision(tMajor, tMinor, tPatch, tBuild);
                        }

                        return VersionToRevision(major, minor, patch, 0);
                    }
                }
            }
        }

        return std::nullopt;
    }

    Revision DetectRevision()
    {
        if (const auto forcedRevisionString = wil::TryGetEnvironmentVariableW(kForceRevisionEnvironmentVariable);
            forcedRevisionString != nullptr)
        {
            if (const auto parsedRevision = VersionStringToRevision(forcedRevisionString.get());
                parsedRevision.has_value())
            {
                return parsedRevision.value();
            }
        }

        if (const auto executableVersion = GetUnityRevisionFromExecutable();
            executableVersion.has_value())
        {
            return executableVersion.value();
        }

        FAIL_FAST_WIN32(ERROR_NOT_FOUND);
    }

    std::optional<Variant> GetUnityVariantFromExecutable()
    {
        const auto executableFilepath = wil::GetModuleFileNameW();
        const auto executableName = std::filesystem::path(executableFilepath.get()).filename().string();
        return ExecutableNameToVariant(executableName);
    }

    std::optional<Variant> GetUnityVariantFromUnityModule()
    {
        const auto module = GetUnityModule();
        const auto unityModuleFilepath = wil::GetModuleFileNameW(module);
        const auto unityModuleName = std::filesystem::path(unityModuleFilepath.get()).filename().string();

        // UnityPlayer.dll is either Runtime or RuntimeDev
        if (EqualsIgnoreCase(unityModuleName, "UnityPlayer.dll") || EqualsIgnoreCase(unityModuleName, "TuanjiePlayer.dll"))
        {
            return Variant::Runtime;
        }
        
        // Unity.dll is always the editor
        // (Unity.exe is checked in the executable check)
        if (EqualsIgnoreCase(unityModuleName, "Unity.dll") || EqualsIgnoreCase(unityModuleName, "Tuanjie.dll"))
        {
            return Variant::Editor;
        }

        return std::nullopt;
    }

    Variant DetectVariant()
    {
        if (const auto forcedVariantString = wil::TryGetEnvironmentVariableW(kForceVariantEnvironmentVariable);
            forcedVariantString != nullptr)
        {
            if (const auto parsedVariant = VariantStringToVariant(forcedVariantString.get());
                parsedVariant.has_value())
            {
                return parsedVariant.value();
            }
        }

        if (const auto executableVariant = GetUnityVariantFromExecutable();
            executableVariant.has_value())
        {
            return executableVariant.value();
        }

        if (const auto unityModuleVariant = GetUnityVariantFromUnityModule();
            unityModuleVariant.has_value())
        {
            return unityModuleVariant.value();
        }

        FAIL_FAST_WIN32(ERROR_NOT_FOUND);
    }

    decltype(&SetWindowLongPtrA) pSetWindowLongPtrA;

    LONG_PTR WINAPI DetourSetWindowLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong)
    {

        wchar_t wcls[256] = {};
        wchar_t wtitle[256] = {};
        GetClassNameW(hWnd, wcls, 256);
        GetWindowTextW(hWnd, wtitle, 256);

        bool isEditorWindow = (wcscmp(wcls, L"UnityGUIViewWndClass") == 0);
        bool isToolbar = (wcsstr(wtitle, L"UnityEditor.Toolbar") != nullptr);

        if (!isEditorWindow || !isToolbar) {
            return pSetWindowLongPtrA(hWnd, nIndex, dwNewLong);
        }

        const auto revision = DetectRevision();
        const auto variant = DetectVariant();

        RunDumper<WindowsDumper>(revision, variant);
        TerminateProcess(GetCurrentProcess(), 0);
        return 0;
    }
}

extern "C" __declspec(dllexport) BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, void *lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        FAIL_FAST_IF_WIN32_ERROR(DetourTransactionBegin());
        *(void **)&pSetWindowLongPtrA = GetProcAddress(GetModuleHandleW(L"USER32"), "SetWindowLongPtrA");
        FAIL_FAST_IF_WIN32_ERROR(DetourAttach((void **)&pSetWindowLongPtrA, &DetourSetWindowLongPtrA));
        FAIL_FAST_IF_WIN32_ERROR(DetourTransactionCommit());
    }

    return TRUE;
}

namespace
{
    struct SectionInfo
    {
        std::string name;
        uintptr_t address;
        size_t size;
        bool executable;
    };

    struct Pattern
    {
        std::vector<uint8_t> bytes;
        std::vector<uint8_t> mask; // 0xFF = compare, 0x00 = wildcard
    };

    std::string Narrow(std::wstring_view value)
    {
        if (value.empty())
            return {};

        const auto size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    HMODULE FindEngineModule()
    {
        for (auto const *name : {L"Tuanjie.exe", L"Tuanjie.dll", L"Unity.exe", L"Unity.dll", L"TuanjiePlayer.dll", L"UnityPlayer.dll"})
        {
            if (const auto module = GetModuleHandleW(name); module != nullptr)
                return module;
        }

        return nullptr;
    }

    Pattern MakePattern(char const *text)
    {
        Pattern pattern;

        auto const hexValue = [](char c) -> int
        {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };

        for (auto const *p = text; *p != '\0';)
        {
            if (*p == ' ')
            {
                ++p;
                continue;
            }

            if (p[0] == '?' && p[1] == '?')
            {
                pattern.bytes.push_back(0);
                pattern.mask.push_back(0);
                p += 2;
                continue;
            }

            const auto hi = hexValue(p[0]);
            const auto lo = hexValue(p[1]);
            if (hi < 0 || lo < 0)
                break;

            pattern.bytes.push_back(static_cast<uint8_t>((hi << 4) | lo));
            pattern.mask.push_back(0xFF);
            p += 2;
        }

        return pattern;
    }

    uintptr_t FindInRange(uint8_t const *begin, uint8_t const *end, Pattern const &pattern)
    {
        if (pattern.bytes.empty() || end <= begin)
            return 0;

        const auto size = static_cast<size_t>(end - begin);
        if (size < pattern.bytes.size())
            return 0;

        auto const *last = begin + (size - pattern.bytes.size());

        for (auto const *cursor = begin; cursor <= last;)
        {
            auto const *hit = static_cast<uint8_t const *>(
                std::memchr(cursor, pattern.bytes[0], static_cast<size_t>(last - cursor) + 1));

            if (hit == nullptr)
                return 0;

            bool matched = true;
            for (size_t i = 1; i < pattern.bytes.size(); ++i)
            {
                if (pattern.mask[i] != 0 && hit[i] != pattern.bytes[i])
                {
                    matched = false;
                    break;
                }
            }

            if (matched)
                return reinterpret_cast<uintptr_t>(hit);

            cursor = hit + 1;
        }

        return 0;
    }

    std::vector<SectionInfo> EnumerateSections(uintptr_t moduleBase)
    {
        std::vector<SectionInfo> sections;

        auto const *dos = reinterpret_cast<IMAGE_DOS_HEADER const *>(moduleBase);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            return sections;

        auto const *nt = reinterpret_cast<IMAGE_NT_HEADERS const *>(moduleBase + static_cast<uintptr_t>(dos->e_lfanew));
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return sections;

        auto const *section = IMAGE_FIRST_SECTION(nt);
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
        {
            auto name = std::string(reinterpret_cast<char const *>(section->Name), 8);
            name = name.c_str(); // trim at first NUL

            sections.push_back(SectionInfo{
                .name = name,
                .address = moduleBase + section->VirtualAddress,
                .size = section->Misc.VirtualSize,
                .executable = (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0,
            });
        }

        return sections;
    }

    size_t GetModuleSize(uintptr_t moduleBase)
    {
        auto const *dos = reinterpret_cast<IMAGE_DOS_HEADER const *>(moduleBase);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            return 0;

        auto const *nt = reinterpret_cast<IMAGE_NT_HEADERS const *>(moduleBase + static_cast<uintptr_t>(dos->e_lfanew));
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return 0;

        return nt->OptionalHeader.SizeOfImage;
    }

    std::string QueryVersionString(std::wstring const &path, wchar_t const *name)
    {
        DWORD handle = 0;
        const auto size = GetFileVersionInfoSizeW(path.c_str(), &handle);
        if (size == 0)
            return {};

        std::vector<uint8_t> data(size);
        if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data()))
            return {};

        struct Translation
        {
            WORD language;
            WORD codePage;
        };

        Translation *translations = nullptr;
        UINT translationSize = 0;
        if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void **>(&translations), &translationSize)
            || translations == nullptr
            || translationSize < sizeof(Translation))
        {
            return {};
        }

        wchar_t block[128];
        swprintf_s(block, L"\\StringFileInfo\\%04x%04x\\%s", translations[0].language, translations[0].codePage, name);

        wchar_t *value = nullptr;
        UINT valueSize = 0;
        if (!VerQueryValueW(data.data(), block, reinterpret_cast<void **>(&value), &valueSize) || value == nullptr)
            return {};

        return Narrow(std::wstring_view(value));
    }

}

namespace typetree_ripper::layout
{

    namespace
    {
        uintptr_t FindFunctionStart(uintptr_t address)
        {
            struct Prologue
            {
                uint8_t bytes[4];
                uint8_t length;
            };

            static constexpr Prologue kPrologues[] = {
                {{0x48, 0x89, 0x5C, 0x24}, 4}, // mov [rsp+8], rbx
                {{0x48, 0x89, 0x4C, 0x24}, 4}, // mov [rsp+8], rcx
                {{0x48, 0x89, 0x54, 0x24}, 4}, // mov [rsp+8], rdx
                {{0x4C, 0x89, 0x44, 0x24}, 4}, // mov [rsp+8], r8
                {{0x4C, 0x89, 0x4C, 0x24}, 4}, // mov [rsp+8], r9
                {{0x40, 0x55, 0x00, 0x00}, 2}, // push rbp
                {{0x48, 0x83, 0xEC, 0x00}, 4}, // sub rsp, imm8
                {{0x48, 0x81, 0xEC, 0x00}, 4}, // sub rsp, imm32
            };

            for (size_t back = 0; back < 0x400; back++)
            {
                const auto position = address - back;
                auto const *bytes = reinterpret_cast<uint8_t const *>(position);

                const auto previous = bytes[-1];
                if (previous != 0xCC && previous != 0xC3 && previous != 0x90)
                    continue;

                for (auto const &prologue : kPrologues)
                {
                    if (std::memcmp(bytes, prologue.bytes, prologue.length) == 0)
                        return position;
                }
            }

            return 0;
        }
    }

    uintptr_t FindTransferVftable()
    {
        auto const module = FindEngineModule();
        if (module == nullptr)
            return 0;

        auto const sections = EnumerateSections(reinterpret_cast<uintptr_t>(module));

        for (auto const *patternText : {"48 89 51 50", "48 89 91 80 00 00 00"})
        {
            auto const pattern = MakePattern(patternText);

            for (auto const &section : sections)
            {
                if (!section.executable)
                    continue;

                auto const *begin = reinterpret_cast<uint8_t const *>(section.address);
                auto const *end = begin + section.size;
                auto const *cursor = begin;

                while (cursor < end)
                {
                    auto const hit = FindInRange(cursor, end, pattern);
                    if (hit == 0)
                        break;

                    cursor = reinterpret_cast<uint8_t const *>(hit) + 1;

                    auto const start = FindFunctionStart(hit);
                    if (start == 0 || start > hit || hit - start >= 0x60)
                        continue;

                    auto const *code = reinterpret_cast<uint8_t const *>(start);

                    for (size_t i = 0; i + 10 <= 0x60; i++)
                    {
                        if (code[i] != 0x48 || code[i + 1] != 0x8D || code[i + 2] != 0x05)
                            continue;

                        int32_t displacement = 0;
                        std::memcpy(&displacement, code + i + 3, sizeof(displacement));

                        auto const target = start + i + 7 + static_cast<uintptr_t>(static_cast<intptr_t>(displacement));

                        for (size_t j = i + 7; j + 3 <= 0x60; j++)
                        {
                            if (code[j] == 0x48 && code[j + 1] == 0x89 && code[j + 2] == 0x01)
                                return target;
                        }
                    }
                }
            }
        }

        return 0;
    }
    
    std::string QueryEngineProductVersion()
    {
        const auto module = FindEngineModule();
        if (module == nullptr)
            return {};

        wchar_t path[MAX_PATH] = {};
        if (GetModuleFileNameW(module, path, MAX_PATH) == 0)
            return {};

        return QueryVersionString(std::wstring(path), L"ProductVersion");
    }
}


extern "C" __declspec(dllexport) void __stdcall StartNativeDump()
{
    typetree_ripper::g_TransferVftable = typetree_ripper::layout::FindTransferVftable();

    const auto revision = DetectRevision();
    const auto variant = DetectVariant();

    RunDumper<WindowsDumper>(revision, variant);

    TerminateProcess(GetCurrentProcess(), 0);
}
