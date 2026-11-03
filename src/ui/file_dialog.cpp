#include "file_dialog.h"

#include "defer.h"
#include "nfd.h"

std::optional<std::filesystem::path> ShowOpenFileDialog(std::string& error) {
    error.clear();

    if (NFD_Init() != NFD_OKAY) {
        error = NFD_GetError();
        return std::nullopt;
    }
    defer(NFD_Quit());

    nfdnchar_t* selectedPath = nullptr;
    nfdopendialognargs_t options{};
    nfdresult_t result = NFD_OpenDialogN_With(&selectedPath, &options);

    if (result == NFD_CANCEL) {
        return std::nullopt;
    }
    if (result == NFD_ERROR) {
        error = NFD_GetError();
        return std::nullopt;
    }

    defer(NFD_FreePathN(selectedPath));
    return std::filesystem::path(selectedPath);
}

std::optional<std::filesystem::path> ShowSaveFileDialog(
    const std::filesystem::path& defaultName, std::string& error) {
    error.clear();

    if (NFD_Init() != NFD_OKAY) {
        error = NFD_GetError();
        return std::nullopt;
    }
    defer(NFD_Quit());

    nfdnchar_t* selectedPath = nullptr;
    nfdsavedialognargs_t options{};
    options.defaultName = defaultName.c_str();
    const nfdresult_t result = NFD_SaveDialogN_With(&selectedPath, &options);

    if (result == NFD_CANCEL) {
        return std::nullopt;
    }
    if (result == NFD_ERROR) {
        error = NFD_GetError();
        return std::nullopt;
    }

    defer(NFD_FreePathN(selectedPath));
    return std::filesystem::path(selectedPath);
}
