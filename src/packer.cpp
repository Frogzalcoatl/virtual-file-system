#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <vfs/packer.hpp>


struct PackerTask {
    VFS_Entry entry;
    std::string systemPath;
    std::string relativePath;
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printf("Usage: vfs <input_path> <output_file> <header_version>\n");
        return 1;
    }
    std::filesystem::path inputPath = argv[1];
    std::filesystem::path outputFilePath = argv[2];
    std::string versionArg = argv[3];
    if (!std::filesystem::exists(inputPath) || !std::filesystem::is_directory(inputPath)) {
        std::cout << "Error: input path is not a valid directory: " << inputPath << std::endl;
        return 1;
    }
    std::vector<PackerTask> tasks;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(inputPath)) {
        if (entry.is_regular_file()) {
            PackerTask task;
            task.systemPath = entry.path().string();
            std::filesystem::path relativePath = std::filesystem::relative(entry.path(), inputPath);
            task.relativePath =
                relativePath.generic_string(); // Forces "/" on all operating systems
            task.entry.id = stringHash(task.relativePath);
            task.entry.size = entry.file_size();
            task.entry.offset = 0; // Computed after sorting
            std::cout << "Found asset: " << task.relativePath << "(" << task.entry.size << " bytes)"
                      << std::endl;
            tasks.push_back(task);
        }
    }
    const size_t fileCount = tasks.size();
    std::cout << "Found " << fileCount << " files to pack" << std::endl;
    std::sort(tasks.begin(), tasks.end(), [](const PackerTask& a, const PackerTask& b) {
        return a.entry.id < b.entry.id;
    });
    uint64_t currentOffset = sizeof(VFS_Header) + (fileCount * sizeof(VFS_Entry));
    for (size_t i = 0; i < fileCount; i++) {
        tasks[i].entry.offset = currentOffset;
        currentOffset += tasks[i].entry.size;
    }
    if (outputFilePath.has_parent_path()) {
        std::filesystem::create_directories(outputFilePath.parent_path());
    }
    std::ofstream outputFile(outputFilePath, std::ios::binary);
    if (!outputFile.is_open()) {
        std::cout << "Error: Could not create output file " << outputFilePath.string() << std::endl;
        return 1;
    }
    VFS_Header header;
    header.magic = PackMagic;
    std::memset(header.version, 0, sizeof(header.version));
    size_t copyLength = std::min(versionArg.size(), sizeof(header.version) - 1);
    std::copy_n(versionArg.begin(), copyLength, header.version);
    header.fileCount = static_cast<uint32_t>(fileCount);
    header._pad = 0;
    outputFile.write(reinterpret_cast<const char*>(&header), sizeof(VFS_Header));
    for (size_t i = 0; i < fileCount; i++) {
        outputFile.write(reinterpret_cast<const char*>(&tasks[i].entry), sizeof(VFS_Entry));
    }
    for (size_t i = 0; i < fileCount; i++) {
        std::ifstream asset(tasks[i].systemPath, std::ios::binary);
        if (!asset.is_open()) {
            std::cout << "Error: Could not open asset file " << tasks[i].systemPath << std::endl;
            return 1;
        }
        std::vector<std::byte> buffer(tasks[i].entry.size);
        asset.read(reinterpret_cast<char*>(buffer.data()), tasks[i].entry.size);
        asset.close();
        outputFile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    }
    outputFile.close();
    std::cout << "Successfully packed " << outputFilePath.string() << std::endl;
}