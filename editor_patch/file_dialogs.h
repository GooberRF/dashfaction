#pragma once

#include <windows.h>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

// Redirects RED's COMDLG32 file dialogs to the modern IFileOpenDialog/IFileSaveDialog.
void ApplyFileDialogPatches();

struct AlpineFileFilter
{
    std::string label;
    std::string spec;
};

// OPENFILENAME filters are label/spec pairs terminated by an empty string, with no length given
// by the caller, so the walk is capped instead of trusted.
inline std::vector<AlpineFileFilter> alpine_file_dialog_split_filter(const char* filter)
{
    constexpr size_t max_strings = 128;
    constexpr size_t max_string_len = 1024;

    std::vector<std::string> parts;
    if (filter) {
        while (parts.size() < max_strings) {
            size_t len = 0;
            while (len < max_string_len && filter[len]) {
                ++len;
            }
            if (len == 0 || filter[len]) {
                break; // end of the list, or a string long enough to be a bad pointer
            }
            parts.emplace_back(filter, len);
            filter += len + 1;
        }
    }

    std::vector<AlpineFileFilter> pairs;
    for (size_t i = 0; i + 1 < parts.size(); i += 2) {
        pairs.push_back({parts[i], parts[i + 1]});
    }
    return pairs;
}

// Call sites mix "v3m" and ".rfg"; IFileDialog::SetDefaultExtension wants no dot.
inline std::string alpine_file_dialog_normalize_defext(const char* defext)
{
    if (!defext) {
        return {};
    }
    while (*defext == '.') {
        ++defext;
    }
    return defext;
}

inline bool alpine_file_dialog_copy_bounded(const char* src, char* dst, size_t size)
{
    if (!src || !dst || size == 0) {
        return false;
    }
    const size_t len = std::strlen(src);
    if (len + 1 > size) {
        return false;
    }
    std::memcpy(dst, src, len + 1);
    return true;
}

inline void alpine_file_dialog_name_offsets(const char* path, WORD& file_offset, WORD& ext_offset)
{
    file_offset = 0;
    ext_offset = 0;
    if (!path) {
        return;
    }
    size_t name = 0;
    size_t ext = 0;
    for (size_t i = 0; path[i]; ++i) {
        if (path[i] == '\\' || path[i] == '/' || path[i] == ':') {
            name = i + 1;
            ext = 0;
        }
        else if (path[i] == '.') {
            ext = i + 1;
        }
    }
    file_offset = static_cast<WORD>(name);
    ext_offset = static_cast<WORD>(ext);
}
