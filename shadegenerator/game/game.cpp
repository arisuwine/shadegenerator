#include "game.hpp"

#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <vdf_parser.hpp>

#include "debug.hpp"

using namespace shade;

namespace {
	enum EPathError : std::uint8_t {
		REG_PATH_NOT_FOUND = 0x1,
		INVALID_PATH_SIZE,
		FAILED_READ_STEAMPATH
	};

	struct PathError_t {
		EPathError m_eError;
		LONG       m_Result;
	};

	std::expected<fs::path, PathError_t> GetSteamPath() {
		HKEY key{};
		LONG result = RegOpenKeyW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", &key);

		if (result != ERROR_SUCCESS)
			return std::unexpected<PathError_t>(PathError_t{ .m_eError = REG_PATH_NOT_FOUND, .m_Result = result });

		DWORD type{};
		DWORD size{};

		const wchar_t* szValue = L"SteamPath";

		result = RegQueryValueExW(key, szValue, nullptr, &type, nullptr, &size);

		if (result != ERROR_SUCCESS || type != REG_SZ) {
			RegCloseKey(key);
			return std::unexpected<PathError_t>(PathError_t{ .m_eError = FAILED_READ_STEAMPATH, .m_Result = result });
		}

		const std::size_t nCharCount = size / sizeof(wchar_t);

		if (nCharCount == 0) {
			RegCloseKey(key);
			return std::unexpected<PathError_t>(PathError_t{ .m_eError = INVALID_PATH_SIZE, .m_Result = ERROR_INVALID_DATA });
		}

		std::wstring szSteamPath;
		szSteamPath.resize(nCharCount);

		result = RegQueryValueExW(key, szValue, nullptr, nullptr, reinterpret_cast<LPBYTE>(szSteamPath.data()), &size);
		RegCloseKey(key);

		if (result != ERROR_SUCCESS)
			return std::unexpected<PathError_t>(PathError_t{ .m_eError = FAILED_READ_STEAMPATH, .m_Result = result });

		if (szSteamPath.back() == L'\0')
			szSteamPath.pop_back();

		return fs::path{ szSteamPath };
	}

	/// @author neverlosecc/source2gen
	std::optional<fs::path> GetGamePath(const fs::path& steamPath, const std::size_t nGameID) {
		const fs::path libraryFoldersPath = steamPath / "steamapps" / "libraryfolders.vdf";
		std::ifstream  libraryFoldersStream{ libraryFoldersPath };
		if (!libraryFoldersStream)
			return std::nullopt;

		bool       parseOk        = false;
		const auto libraryFolders = tyti::vdf::read(libraryFoldersStream, &parseOk);
		if (!parseOk)
			return std::nullopt;

		const std::string szGameID = std::to_string(nGameID);

		for (const auto& [_, info] : libraryFolders.childs) {
			if (!info)
				continue;

			const auto appsIt = info->childs.find("apps");
			if (appsIt == info->childs.end() || !appsIt->second || !appsIt->second->attribs.contains(szGameID))
				continue;

			const auto libraryPathIt = info->attribs.find("path");
			if (libraryPathIt == info->attribs.end())
				continue;

			const fs::path  steamAppsPath = fs::path{ libraryPathIt->second } / "steamapps";
			const fs::path  manifestPath  = steamAppsPath / std::format("appmanifest_{}.acf", szGameID);
			std::error_code existsError;
			if (!fs::is_regular_file(manifestPath, existsError))
				continue;

			std::ifstream manifestStream{ manifestPath };
			if (!manifestStream)
				continue;

			const auto manifest = tyti::vdf::read(manifestStream, &parseOk);
			if (!parseOk)
				continue;

			const auto installDirIt = manifest.attribs.find("installdir");
			if (installDirIt == manifest.attribs.end())
				continue;

			fs::path gamePath = steamAppsPath / "common" / installDirIt->second;
			if (fs::is_directory(gamePath, existsError))
				return std::move(gamePath);
		}

		return std::nullopt;
	}
} // namespace

game::CGame::CGame(std::string_view szPath) {
	if (!szPath.empty()) {
		m_GamePath = { szPath };
		return;
	}

	const auto steamPath = GetSteamPath();
	if (!steamPath) {
		const auto& error = steamPath.error();
		throw std::runtime_error(std::format("error code: 0x{:x}, result: 0x{:x}", static_cast<uint32_t>(error.m_eError), error.m_Result));
	}

#ifdef SHADE_GAME_CS2
	const std::size_t nGameID = 730;
#elif defined(SHADE_GAME_DOTA2)
	const std::size_t nGameID = 570;
#elif defined(SHADE_GAME_DEADLOCK)
	const std::size_t nGameID = 1422450;
#endif

	const auto gamePath = GetGamePath(steamPath.value(), nGameID);
	if (!gamePath)
		throw std::runtime_error(std::format("failed to get game (id: {}) path", nGameID));

	m_GamePath = *gamePath;
}

std::vector<game::CGame::ModuleEntry_t> game::CGame::GetModuleEntries() const {
	std::string szEnginePath      = "\\bin\\win64";
	std::string szEngineSubFolder = "game" + szEnginePath;
#ifdef SHADE_GAME_CS2
	std::string szGameSubFolder = "game\\csgo" + szEnginePath;
#elif defined(SHADE_GAME_DOTA2)
	std::string szGameSubFolder = "game\\dota" + szEnginePath;
#elif defined(SHADE_GAME_DEADLOCK)
	std::string szGameSubFolder = "game\\citadel" + szEnginePath;
#endif

	std::vector<ModuleEntry_t> entries = { { "client.dll", szGameSubFolder },
		                                   { "host.dll", szGameSubFolder },
		                                   { "server.dll", szGameSubFolder },
		                                   { "engine2.dll", szEngineSubFolder },
		                                   { "schemasystem.dll", szEngineSubFolder },
		                                   { "tier0.dll", szEngineSubFolder },
		                                   { "animationsystem.dll", szEngineSubFolder },
		                                   { "materialsystem2.dll", szEngineSubFolder },
		                                   { "meshsystem.dll", szEngineSubFolder },
		                                   { "networksystem.dll", szEngineSubFolder },
		                                   { "panorama.dll", szEngineSubFolder },
		                                   { "particles.dll", szEngineSubFolder },
		                                   { "pulse_system.dll", szEngineSubFolder },
		                                   { "rendersystemdx11.dll", szEngineSubFolder },
		                                   { "resourcesystem.dll", szEngineSubFolder },
		                                   { "scenefilecache.dll", szEngineSubFolder },
		                                   { "scenesystem.dll", szEngineSubFolder },
		                                   { "soundsystem.dll", szEngineSubFolder },
		                                   { "vphysics2.dll", szEngineSubFolder },
		                                   { "worldrenderer.dll", szEngineSubFolder },
		                                   { "assetpreview.dll", szEngineSubFolder },
#ifdef SHADE_GAME_CS2
		                                   { "matchmaking.dll", szGameSubFolder }
#elif defined(SHADE_GAME_DOTA2)
		                                   { "navsystem.dll", szEngineSubFolder }
#endif
	};

	return entries;
}

[[nodiscard]] std::vector<std::pair<std::string, HMODULE>> game::CGame::LoadModules() const {
	const auto entries = GetModuleEntries();
	for (const auto& [_, szRelativePath] : entries) {
		const fs::path path = m_GamePath / szRelativePath;
		if (!AddDllDirectory(path.c_str()))
			throw std::runtime_error(std::format("failed to add dll directory, path: {}", path.string()));
	}

	std::vector<std::pair<std::string, HMODULE>> modules;
	modules.reserve(entries.size());

	auto LoadModule = [](const fs::path& path, std::string_view szName) {
		const HMODULE hModule = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
		if (!hModule) {
			const DWORD error = GetLastError();
			throw std::runtime_error(std::format("failed to load {} module, path: {}, last error: 0x{:x}", szName, path.string(), error));
		}

		lg::Success("game", "loaded {} module", szName);

		return hModule;
	};

	for (const auto& [name, path] : entries)
		modules.push_back(std::pair{ std::string(name), LoadModule(m_GamePath / path / name, name) });

	return modules;
}
