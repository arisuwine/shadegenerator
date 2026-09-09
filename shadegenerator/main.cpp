#include <exception>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <Windows.h>

#include <CLI/CLI.hpp>

#include "config.hpp"
#include "generation.hpp"

#include "game/game.hpp"

#include "sdk/interfacesystem/interfacesystem.hpp"

#include "sdk/schemasystem/schemasystem.hpp"

#include "utils/debug.hpp"

int main(int argc, char* argv[]) {
	try {
		CLI::App app{ "Shade Generator - Source 2 SDK Generator" };
		app.set_version_flag("-v,--version", std::string{ "Shade Generator " } + SHADEGENERATOR_VERSION);

		const std::map<std::string, shade::EEmitType> types = {
			{ "CPP", shade::EEmitType::CPP },
			{ "IDA_C", shade::EEmitType::IDA_C },
			{ "IDA_CPP", shade::EEmitType::IDA_CPP },
		};

		shade::EEmitType eEmitType;
		app.add_option("-t,--type", eEmitType, "SDK emit type (CPP is default)")
		    ->default_val(shade::EEmitType::CPP)
		    ->transform(CLI::CheckedTransformer(types, CLI::ignore_case))
		    ->type_name("EMIT TYPE");

		std::string szGamePath;
		app.add_option("-p,--path", szGamePath, "Target install directory (necessary if the path to the game was not detected automatically)");

		std::string szOutputPath;
		app.add_option("-o,--output", szOutputPath, "Output directory for generated files (defaults to the executable directory)");

		CLI11_PARSE(app, argc, argv);

		shade::game::CGame game{ szGamePath };

		const auto LoadedModules = game.LoadModules();

		g_pSchemaSystem = CInterfaceSystem::Get<CSchemaSystem>("schemasystem.dll", "SchemaSystem_001");
		if (!g_pSchemaSystem)
			throw std::runtime_error("failed to resolve schemasystem interface");

		for (const auto& [name, module] : LoadedModules) {
			auto result = g_pSchemaSystem->InstallSchemaBinding(module);
			if (!result) {
				switch (result.error()) {
				case INVALID_FUNC_ADDRESS:
					lg::Warn("schemasystem", "no schema bindings in {} module", name);
					break;
				case BAD_RESULT:
					throw std::runtime_error(std::format("failed to install schema binding for {} module", name));
				}
			} else
				lg::Success("schemasystem", "successfully installed schema binding for {} module", name);
		}

		shade::GeneratorConfig_t ctx;

		if (szOutputPath.empty()) {
			std::vector<wchar_t> executablePathBuffer(MAX_PATH);
			for (;;) {
				const DWORD pathLength = GetModuleFileNameW(nullptr, executablePathBuffer.data(), static_cast<DWORD>(executablePathBuffer.size()));
				if (pathLength == 0)
					throw std::runtime_error("failed to determine executable path");

				if (pathLength < executablePathBuffer.size()) {
					ctx.m_OutputPath = fs::path{ std::wstring_view{ executablePathBuffer.data(), pathLength } }.parent_path();
					break;
				}

				executablePathBuffer.resize(executablePathBuffer.size() * 2);
			}
		} else
			ctx.m_OutputPath = fs::path{ szOutputPath };

		ctx.m_eEmitType = eEmitType;

		shade::GenerateSdk(ctx);
	} catch (const std::exception& e) {
		lg::Error("", "runtime error occurred: {}", e.what());
		return 1;
	} catch (...) {
		lg::Error("", "unknown runtime error occurred");
		return 1;
	}

	return 0;
}
