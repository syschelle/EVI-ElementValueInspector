#pragma once

#include "Models.h"
#include "UiLanguage.h"

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>

class DicomScanner {
public:
    using ProgressCallback = std::function<void(std::size_t, std::size_t, const std::filesystem::path&)>;

    std::shared_ptr<ScanResult> Scan(
        const std::filesystem::path& root,
        uint16_t group,
        uint16_t element,
        std::atomic_bool& cancel,
        UiLanguage language,
        ProgressCallback progress);
};
