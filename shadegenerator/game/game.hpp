#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <Windows.h>

namespace fs = std::filesystem;

namespace shade::game {
	/** @brief Locates a supported Source 2 installation and loads its schema modules. */
	class CGame {
	private:
		fs::path m_GamePath;

	private:
		struct ModuleEntry_t {
			std::string_view m_szModuleName;
			std::string      m_szSubFolder;
		};

		[[nodiscard]] std::vector<ModuleEntry_t> GetModuleEntries() const;

	public:
		/**
		 * @brief Creates a game loader for an explicit or automatically detected installation.
		 * @param szPath Game installation directory, or an empty string to search Steam libraries.
		 * @throws std::runtime_error if the game installation cannot be located.
		 */
		CGame(std::string_view szPath = "");

	public:
		/**
		 * @brief Loads the modules that provide schema bindings for the selected game.
		 * @return Pairs containing module names and their Windows module handles.
		 * @throws std::runtime_error if a required module cannot be loaded.
		 */
		[[nodiscard]] std::vector<std::pair<std::string, HMODULE>> LoadModules() const;
	};
} // namespace shade::game
