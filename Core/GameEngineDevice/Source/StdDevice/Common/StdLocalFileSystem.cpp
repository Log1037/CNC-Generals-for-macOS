/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

///////// StdLocalFileSystem.cpp /////////////////////////
// Stephan Vedder, April 2025
////////////////////////////////////////////////////////////

#include "Common/AsciiString.h"
#include "Common/GameMemory.h"
#include "Common/PerfTimer.h"
#include "StdDevice/Common/StdLocalFileSystem.h"
#include "StdDevice/Common/StdLocalFile.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

#ifndef _WIN32
// GeneralsX @bugfix felipebraz 23/03/2026 Asset root fallback path for loose file lookups.
// On Linux/macOS the game binary's cwd and the data directory (asset root, CNC_GENERALS_ZH_PATH) are separate.
// The StdBIGFileSystem sets this after resolving the primary asset directory so that relative paths like
// "Data\Scripts\SkirmishScripts.scb" can be found in the asset root when the cwd lookup fails.
static std::filesystem::path s_assetFallbackPath;
#endif

StdLocalFileSystem::StdLocalFileSystem() : LocalFileSystem()
{
}

StdLocalFileSystem::~StdLocalFileSystem() {
}

//DECLARE_PERF_TIMER(StdLocalFileSystem_openFile)
static std::filesystem::path fixFilenameFromWindowsPath(const Char *filename, Int access)
{
	std::string fixedFilename(filename);

#ifndef _WIN32
	// Replace backslashes with forward slashes on unix
	std::replace(fixedFilename.begin(), fixedFilename.end(), '\\', '/');
#endif

	// Convert the filename to a std::filesystem::path and pass that
	std::filesystem::path path(std::move(fixedFilename));

#ifndef _WIN32
	// check if the file exists to see if fixup is required
	// if it's not found try to match disregarding case sensitivity
	// For cases where a write is happening, we should check if the parent path exists, if so, let it through, since the file may not exist yet.
	std::error_code ec;
	if (!std::filesystem::exists(path, ec) &&
		((!(access & File::WRITE)) || ((access & File::WRITE) && !std::filesystem::exists(path.parent_path(), ec))))
	{
		// GeneralsX @bugfix felipebraz 23/03/2026 Before attempting expensive case-insensitive cwd traversal,
		// check if the relative path resolves directly from the asset root (e.g. CNC_GENERALS_ZH_PATH).
		// On Windows cwd == install dir so this is never needed; on Linux/macOS they are separate.
		if (!s_assetFallbackPath.empty() && path.is_relative()) {
			std::filesystem::path assetRootPath = s_assetFallbackPath / path;
			std::error_code ecAsset;
			const bool writeAndParentExists = (access & File::WRITE) && std::filesystem::exists(assetRootPath.parent_path(), ecAsset);
			if (std::filesystem::exists(assetRootPath, ecAsset) || writeAndParentExists) {
				return assetRootPath;
			}

			#ifdef __linux__
			// GeneralsX @bugfix BenderAI 11/05/2026 Linux: resolve case-insensitive paths from asset root.
			// Some cursor files are lowercase on disk (e.g. sccpointer.ani) while INI references mixed-case names.
			// The existing case-insensitive traversal below only checks cwd, not the asset root fallback.
			std::filesystem::path assetRootFixed = s_assetFallbackPath;
			std::filesystem::path assetRootCurrent = s_assetFallbackPath;
			bool assetRootFound = true;
			for (const auto& p : path)
			{
				std::filesystem::path pathFixedPart;
				std::error_code ecAssetCase;
				if (std::filesystem::exists(assetRootCurrent / p, ecAssetCase))
				{
					pathFixedPart = p;
				}
				else
				{
					for (auto& entry : std::filesystem::directory_iterator(assetRootCurrent, ecAssetCase))
					{
						if (strcasecmp(entry.path().filename().string().c_str(), p.string().c_str()) == 0)
						{
							pathFixedPart = entry.path().filename();
							break;
						}
					}
				}

				if (pathFixedPart.empty())
				{
					assetRootFound = false;
					break;
				}

				assetRootFixed /= pathFixedPart;
				assetRootCurrent /= pathFixedPart;
			}

			if (assetRootFound)
			{
				std::error_code ecAssetFixed;
				const bool writeAndParentExistsFixed = (access & File::WRITE)
					&& std::filesystem::exists(assetRootFixed.parent_path(), ecAssetFixed);
				if (std::filesystem::exists(assetRootFixed, ecAssetFixed) || writeAndParentExistsFixed)
				{
					return assetRootFixed;
				}
			}
			#endif
		}
		// Traverse path to try and match case-insensitively
		std::filesystem::path parent = path.parent_path();

		std::filesystem::path pathFixed;
		std::filesystem::path pathCurrent;
		// GeneralsX @build felipebraz 20/06/2025 const auto& required because libc++ std::filesystem::path iterator yields temporaries (non-const lvalue reference would fail on Apple clang)
		for (const auto& p : path)
		{
			std::filesystem::path pathFixedPart;
			if (pathCurrent.empty())
			{
				// Load the first part of the path
				pathFixed /= p;
				pathCurrent /= p;
				continue;
			}

			if (std::filesystem::exists(pathCurrent / p, ec))
			{
				pathFixedPart = p;
			}
			else if (std::filesystem::exists(pathFixed / p, ec))
			{
				pathFixedPart = p;
			}
			else
			{
				// Check if the subpath exists using case-insensitive comparison
				for (auto& entry : std::filesystem::directory_iterator(pathFixed, ec))
				{
					if (strcasecmp(entry.path().filename().string().c_str(), p.string().c_str()) == 0)
					{
						pathFixedPart = entry.path().filename();
						break;
					}
				}
			}

			if (pathFixedPart.empty())
			{
				// Required to allow creation of new files
				if (!(access & File::WRITE))
				{
					DEBUG_LOG(("StdLocalFileSystem::fixFilenameFromWindowsPath - Error finding file %s", filename.string().c_str()));
					DEBUG_LOG(("StdLocalFileSystem::fixFilenameFromWindowsPath - Got so far %s", pathCurrent.string().c_str()));

					return std::filesystem::path();
				}

				// Use the last known good path
				pathFixed = p;
			}

			// Copy of the current path to mirror the current depth
			pathFixed /= pathFixedPart;
			pathCurrent /= p;
		}
		path = pathFixed;
	}
#endif

	return path;
}

File * StdLocalFileSystem::openFile(const Char *filename, Int access, size_t bufferSize)
{
	//USE_PERF_TIMER(StdLocalFileSystem_openFile)

	// sanity check
	if (strlen(filename) <= 0) {
		return nullptr;
	}

	std::filesystem::path path = fixFilenameFromWindowsPath(filename, access);

	if (path.empty()) {
		return nullptr;
	}

	if (access & File::WRITE) {
		// if opening the file for writing, we need to make sure the directory is there
		// before we try to create the file.
		std::filesystem::path dir = path.parent_path();
		std::error_code ec;
		if (!std::filesystem::exists(dir, ec) || ec) {
			if(!std::filesystem::create_directories(dir, ec) || ec) {
				DEBUG_LOG(("StdLocalFileSystem::openFile - Error creating directory %s", dir.string().c_str()));
				return nullptr;
			}
		}
	}

	StdLocalFile *file = newInstance( StdLocalFile );

	if (file->open(path.string().c_str(), access, bufferSize) == FALSE) {
		deleteInstance(file);
		file = nullptr;
	} else {
		file->deleteOnClose();
	}

// this will also need to play nice with the STREAMING type that I added, if we ever enable this

// srj sez: this speeds up INI loading, but makes BIG files unusable.
// don't enable it without further tweaking.
//
// unless you like running really slowly.
//	if (!(access&File::WRITE)) {
//		// Return a ramfile.
//		RAMFile *ramFile = newInstance( RAMFile );
//		if (ramFile->open(file)) {
//			file->close(); // is deleteonclose, so should delete.
//			ramFile->deleteOnClose();
//			return ramFile;
//		}	else {
//			ramFile->close();
//			deleteInstance(ramFile);
//		}
//	}

	return file;
}

void StdLocalFileSystem::update()
{
}

void StdLocalFileSystem::init()
{
}

void StdLocalFileSystem::reset()
{
}

//DECLARE_PERF_TIMER(StdLocalFileSystem_doesFileExist)
Bool StdLocalFileSystem::doesFileExist(const Char *filename) const
{
	std::filesystem::path path = fixFilenameFromWindowsPath(filename, 0);
	if(path.empty()) {
		return FALSE;
	}

	std::error_code ec;
	return std::filesystem::exists(path, ec);
}

void StdLocalFileSystem::getFileListInDirectory(const AsciiString& currentDirectory, const AsciiString& originalDirectory, const AsciiString& searchName, FilenameList & filenameList, Bool searchSubdirectories) const
{
	AsciiString asciiSearch = originalDirectory;
	asciiSearch.concat(currentDirectory);
	if (asciiSearch.isEmpty()) {
		asciiSearch = ".";
	}

	std::string fixedDirectory(asciiSearch.str());

#ifndef _WIN32
	// Replace backslashes with forward slashes on unix
	std::replace(fixedDirectory.begin(), fixedDirectory.end(), '\\', '/');
#endif

	const std::filesystem::path directory(fixedDirectory);
	const std::string mask(searchName.str());
	auto wildcardMatches = [](const std::string& filename, const std::string& pattern) {
		size_t filenameIndex = 0;
		size_t patternIndex = 0;
		size_t starIndex = std::string::npos;
		size_t retryFilenameIndex = 0;

		while (filenameIndex < filename.size()) {
			if (patternIndex < pattern.size() &&
				(pattern[patternIndex] == '?' ||
				 std::tolower(static_cast<unsigned char>(pattern[patternIndex])) ==
					 std::tolower(static_cast<unsigned char>(filename[filenameIndex])))) {
				++filenameIndex;
				++patternIndex;
			}
			else if (patternIndex < pattern.size() && pattern[patternIndex] == '*') {
				starIndex = patternIndex++;
				retryFilenameIndex = filenameIndex;
			}
			else if (starIndex != std::string::npos) {
				patternIndex = starIndex + 1;
				filenameIndex = ++retryFilenameIndex;
			}
			else {
				return false;
			}
		}

		while (patternIndex < pattern.size() && pattern[patternIndex] == '*') {
			++patternIndex;
		}
		return patternIndex == pattern.size();
	};

	auto addMatchingFile = [&](const std::filesystem::directory_entry& entry) {
		std::error_code entryError;
		if (!entry.is_regular_file(entryError) || entryError) {
			return;
		}

		const std::string filename = entry.path().filename().string();
		// Match Windows' historical treatment of *.* as "all files".
		if (mask != "*.*" && !wildcardMatches(filename, mask)) {
			return;
		}

		AsciiString newFilename(entry.path().string().c_str());
		filenameList.insert(newFilename);
	};

	std::error_code ec;
	if (searchSubdirectories) {
		std::filesystem::recursive_directory_iterator iter(
			directory, std::filesystem::directory_options::skip_permission_denied, ec);
		const std::filesystem::recursive_directory_iterator end;
		if (ec) {
			DEBUG_LOG(("StdLocalFileSystem::getFileListInDirectory - Error opening directory %s", fixedDirectory.c_str()));
			return;
		}

		while (iter != end) {
			addMatchingFile(*iter);
			iter.increment(ec);
			if (ec) {
				DEBUG_LOG(("StdLocalFileSystem::getFileListInDirectory - Error scanning directory %s", fixedDirectory.c_str()));
				ec.clear();
			}
		}
	}
	else {
		std::filesystem::directory_iterator iter(
			directory, std::filesystem::directory_options::skip_permission_denied, ec);
		const std::filesystem::directory_iterator end;
		if (ec) {
			DEBUG_LOG(("StdLocalFileSystem::getFileListInDirectory - Error opening directory %s", fixedDirectory.c_str()));
			return;
		}

		while (iter != end) {
			addMatchingFile(*iter);
			iter.increment(ec);
			if (ec) {
				DEBUG_LOG(("StdLocalFileSystem::getFileListInDirectory - Error scanning directory %s", fixedDirectory.c_str()));
				ec.clear();
			}
		}
	}
}

Bool StdLocalFileSystem::getFileInfo(const AsciiString& filename, FileInfo *fileInfo) const
{
	std::filesystem::path path = fixFilenameFromWindowsPath(filename.str(), 0);

	if(path.empty()) {
		return FALSE;
	}

	std::error_code ec;
	auto file_size = std::filesystem::file_size(path, ec);
	if (ec)
	{
		return FALSE;
	}

	auto write_time = std::filesystem::last_write_time(path, ec);
	if (ec)
	{
		return FALSE;
	}

	// TODO: fix this to be win compatible (time since 1601)
	auto time = write_time.time_since_epoch().count();
	fileInfo->timestampHigh = time >> 32;
	fileInfo->timestampLow = time & UINT32_MAX;
	fileInfo->sizeHigh      = file_size >> 32;
	fileInfo->sizeLow  = file_size & UINT32_MAX;

	return TRUE;
}

Bool StdLocalFileSystem::createDirectory(AsciiString directory)
{
	bool result = FALSE;

	std::string fixedDirectory(directory.str());

#ifndef _WIN32
	// Replace backslashes with forward slashes on unix
	std::replace(fixedDirectory.begin(), fixedDirectory.end(), '\\', '/');
#endif

	if ((!fixedDirectory.empty()) && (fixedDirectory.length() < _MAX_DIR)) {
		// Convert to host path
		std::filesystem::path path(std::move(fixedDirectory));

		std::error_code ec;
		result = std::filesystem::create_directory(path, ec);
		if (ec) {
			result = FALSE;
		}
	}
	return result;
}

AsciiString StdLocalFileSystem::normalizePath(const AsciiString& filePath) const
{
	std::string nonNormalized(filePath.str());
#ifndef _WIN32
	// Replace backslashes with forward slashes on non-Windows platforms
	// GeneralsX @bugfix BenderAI 13/02/2026 Fixed typo: unNormalized → nonNormalized
	std::replace(nonNormalized.begin(), nonNormalized.end(), '\\', '/');
#endif
	std::filesystem::path pathNonNormalized(nonNormalized);
	return AsciiString(pathNonNormalized.lexically_normal().string().c_str());
}

#ifndef _WIN32
// GeneralsX @bugfix felipebraz 23/03/2026 Receive the asset root path from StdBIGFileSystem after it resolves
// CNC_GENERALS_ZH_PATH. Used as a fallback in fixFilenameFromWindowsPath so that loose data files
// (e.g. Data\Scripts\SkirmishScripts.scb) can be found even when cwd != asset root directory.
void StdLocalFileSystem::setAssetRootPath(const AsciiString& path)
{
	std::string p(path.str());
	std::replace(p.begin(), p.end(), '\\', '/');
	s_assetFallbackPath = std::filesystem::path(std::move(p));
	DEBUG_LOG(("StdLocalFileSystem::setAssetRootPath - asset fallback path set to '%s'", s_assetFallbackPath.string().c_str()));
}
#endif
