#ifndef _SYSCTL_Header
#define _SYSCTL_Header


#include <iostream>
#include <math.h>
#include <vector>
#include <algorithm>

#include <string>
#include <filesystem>

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace fs = std::filesystem;

using namespace std;





[[maybe_unused]] static string getlocationstring(const string& str)
{
    return str;
}
[[maybe_unused]] static string loadfilestring(const string& file)
{
    FILE* rf = nullptr;
    if (fopen_s(&rf, file.c_str(), "rb") != 0 || rf == nullptr)
        return "";
    fseek(rf, 0, SEEK_END);
    int filesize = ftell(rf);
    char* pcharget = new char[filesize + 10];
    memset(pcharget, 0, filesize + 10);
    rewind(rf);
    fread((char*)(pcharget), filesize, 1, rf);
    string qfilereads = pcharget;
    delete[]pcharget;
    fclose(rf);
    return qfilereads;
}
// 鏌ユ壘绗﹀悎鏉′欢鐨勬枃浠?
static std::vector<std::string> findFiles(const std::vector<std::string>& files, const std::string& text) {
    std::vector<std::string> result;
    for (const auto& file : files) {
        // 杩欓噷绠€鍗曞亣璁炬枃鏈尮閰嶆槸鏂囦欢鍚嶅寘鍚寚瀹氭枃鏈?
        if (file.find(text) != std::string::npos) {
            result.push_back(file);
        }
    }
    return result;
}
// 鏌ユ壘鎸囧畾鐩綍涓嬬鍚堟枃浠跺悕鐨勬枃浠?
[[maybe_unused]] static std::vector<std::string> DirFileFind(const std::string& path, const std::string& filename) {
    std::string filenameto = filename;
    std::string text;
    std::vector<std::string> files;

    if (filenameto.empty()) {
        filenameto = "*";
    }

    try {
        // 閬嶅巻鎸囧畾鐩綍涓嬬殑鎵€鏈夋枃浠?
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                std::string fullFileName = path +string("\\") + entry.path().filename().string();
                std::string currentFileName = entry.path().filename().string();
                std::string currentFileExt = entry.path().extension().string();
                if (filenameto == "*" || 
                    currentFileName == filenameto ||
                    currentFileExt== filenameto )
                {
                    files.push_back(fullFileName);
                }
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        // 澶勭悊鏂囦欢绯荤粺鎿嶄綔寮傚父
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }

    if (!text.empty()) {
        // 杩囨护绗﹀悎鏂囨湰鏉′欢鐨勬枃浠?
        files = findFiles(files, text);
    }

    return files;
}

[[maybe_unused]] static std::string getFullFileName(const std::string& filename) {
    // 鍒涘缓涓€涓?std::filesystem::path 瀵硅薄
    fs::path path(filename);

    // 鑾峰彇鏂囦欢鍚嶏紙涓嶅寘鍚墿灞曞悕锛?
    std::string baseName = path.stem().string();

    // 鑾峰彇鏂囦欢鎵╁睍鍚?
    std::string extension = path.extension().string();

    // 濡傛灉鎵╁睍鍚嶄互鐐瑰紑澶达紝鍘绘帀寮€澶寸殑鐐?
    if (!extension.empty() && extension[0] == '.') {
        extension = extension.substr(1);
    }

    // 鎷兼帴鏂囦欢鍚嶅拰鎵╁睍鍚?
    std::string fullFileName = baseName;
    if (!extension.empty()) {
        fullFileName += "." + extension;
    }

    return fullFileName;
}

[[maybe_unused]] static std::vector<std::string> splitstring(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);

    while (getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}



#endif //_SYSCTL_Header

