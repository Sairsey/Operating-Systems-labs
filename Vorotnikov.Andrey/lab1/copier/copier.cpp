/// @file
/// @brief Определение класса копирователя
/// @author Воротников Андрей

#include "copier.h"

#include <syslog.h>
#include <algorithm>
#include <filesystem>

bool Copier::UpdateCopyInfo(const std::vector<CopyInfo>& copyInfoList)
{
    std::set<std::string> sources;
    std::map<std::string, DstDirectory> dstDirectories;
    for (const auto& copyInfo : copyInfoList)
    {
        auto dstDir = dstDirectories.find(copyInfo.dst);
        if (dstDirectories.cend() == dstDir)
        {
            dstDirectories[copyInfo.dst] = DstDirectory();
            dstDir = dstDirectories.find(copyInfo.dst);
        }
        auto& dstDirContent = dstDir->second;
        auto srcDir = dstDirContent.find(copyInfo.src);
        if (dstDirContent.cend() == srcDir)
        {
            dstDirContent[copyInfo.src] = ExtSubfolders();
            srcDir = dstDirContent.find(copyInfo.src);
        }
        sources.insert(copyInfo.src);
        auto& extSubfolders = srcDir->second;
        std::string dottedExtension = "." + copyInfo.extension;
        auto extension = extSubfolders.find(dottedExtension);
        if (extSubfolders.cend() == extension)
            extSubfolders[dottedExtension] = {copyInfo.subfolder};
        else
            extension->second.insert(copyInfo.subfolder);
    }
    for (const auto& dst : dstDirectories)
        if (sources.cend() != std::find(sources.cbegin(), sources.cend(), dst.first))
            return false;
    std::lock_guard lock(mtx_);
    dstDirectories_ = dstDirectories;
    return true;
}

bool Copier::Copy()
{
    const std::set<std::string> othersSubdirs = {othersSubdirectory};

    bool success = true;
    std::lock_guard lock(mtx_);
    for (const auto& [dstDirName, dstDirContent] : dstDirectories_)
    {
        try
        {
            auto dstDirPath = std::filesystem::path(dstDirName);
            std::filesystem::remove_all(dstDirPath);
            std::filesystem::create_directory(dstDirPath);
            for (const auto& [srcDirName, srcDirContent] : dstDirContent)
                for (const auto& entry : std::filesystem::directory_iterator(srcDirName))
                {
                    if (!entry.is_regular_file())
                        continue;
                    auto path = entry.path();
                    auto subdirs = &othersSubdirs;
                    auto extensionIt = srcDirContent.find(path.extension());
                    if (srcDirContent.cend() != extensionIt)
                        subdirs = &extensionIt->second;
                    for (const auto& subdir : *subdirs)
                    {
                        auto subdirPath = dstDirPath / subdir;
                        std::filesystem::create_directory(subdirPath);
                        std::filesystem::copy_file(path, subdirPath / path.filename(), std::filesystem::copy_options::skip_existing);
                    }
                }
        }
        catch(...)
        {
            syslog(LOG_ERR, "Failed to copy files to '%s'", dstDirName.c_str());
            success = false;
        }
    }
    return success;
}
