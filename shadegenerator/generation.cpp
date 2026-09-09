#include "generation.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "codegen/cpp_emitter.hpp"
#include "codegen/generator.hpp"
#include "codegen/ida_emitter.hpp"
#include "codegen/outputfile.hpp"
#include "codegen/typeformatter.hpp"

#include "tools/collector/collector.hpp"
#include "tools/dependencies.hpp"
#include "tools/sort.hpp"

#include "schema/dependencies.hpp"
#include "schema/model.hpp"

#include "utils/debug.hpp"

#include "sdk/schemasystem/schemasystem.hpp"

namespace {
	using namespace shade;

	static constexpr std::string_view kszCMakeLists =
	    R"(cmake_minimum_required(VERSION 3.20)

project(shade LANGUAGES CXX)

add_library(shade INTERFACE)

target_include_directories(shade INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_compile_features(shade INTERFACE cxx_std_23))";

	[[nodiscard]] bool IsSafePathComponent(std::string_view component, bool allowEmpty, bool allowColon = false) {
		if (component.empty())
			return allowEmpty;
		if (component == "." || component == ".." || component.back() == ' ' || component.back() == '.')
			return false;

		const std::string_view forbidden = allowColon ? "<>\"/\\|?*" : "<>:\"/\\|?*";
		return component.find_first_of(forbidden) == std::string_view::npos && component.find('\0') == std::string_view::npos;
	}

	void ValidateOutputName(const schema::SchemaTypeName_t& name) {
		if (!IsSafePathComponent(name.m_szModule, true))
			throw std::runtime_error(std::format("unsafe schema module name for output path: {}", name.m_szModule));
		if (!IsSafePathComponent(name.m_szName, false, true))
			throw std::runtime_error(std::format("unsafe schema type name for output path: {}", name.m_szName));
	}

	[[nodiscard]] fs::path TypeOutputPath(const fs::path& sdkPath, const schema::SchemaTypeName_t& name) {
		ValidateOutputName(name);
		if (!IsSafePathComponent(name.m_szName, false))
			throw std::runtime_error(std::format("unsafe formatted schema type name for output path: {}", name.m_szName));
		const fs::path fileName = name.m_szName + ".hpp";
		return name.m_szModule.empty() ? sdkPath / fileName : sdkPath / name.m_szModule / fileName;
	}

	struct CppOutputPlan_t {
		fs::path              m_CMakeListsPath;
		fs::path              m_TypesPath;
		std::vector<fs::path> m_EnumPaths;
		std::vector<fs::path> m_ClassPaths;
	};

	[[nodiscard]] std::string NormalizeOutputPathKey(const fs::path& path) {
		std::string key = path.lexically_normal().generic_string();
		std::ranges::transform(key, key.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return key;
	}

	[[nodiscard]] CppOutputPlan_t PlanCppOutput(const GeneratorConfig_t& config, const schema::CSchemaModel& model) {
		const fs::path  shadePath = config.m_OutputPath / "shade";
		const fs::path  sdkPath   = shadePath / "sdk";
		CppOutputPlan_t plan{
			.m_CMakeListsPath = shadePath / "CMakeLists.txt",
			.m_TypesPath      = sdkPath / "types.hpp",
			.m_EnumPaths      = {},
			.m_ClassPaths     = {},
		};
		plan.m_EnumPaths.reserve(model.GetEnums().size());
		plan.m_ClassPaths.reserve(model.GetClasses().size());

		std::unordered_map<std::string, std::string> owners;

		auto addPath = [&](const fs::path& path, std::string owner) {
			const auto [position, inserted] = owners.try_emplace(NormalizeOutputPathKey(path), owner);
			if (!inserted)
				throw std::runtime_error(std::format("C++ output path collision between {} and {} at {}", position->second, owner, path.string()));
		};

		addPath(plan.m_TypesPath, "reserved types.hpp");
		for (const auto& record : model.GetEnums()) {
			const auto path = TypeOutputPath(sdkPath, record.m_Name);
			addPath(path, std::format("enum {}::{}", record.m_Name.m_szModule, record.m_Name.m_szName));
			plan.m_EnumPaths.push_back(path);
		}

		for (const auto& record : model.GetClasses()) {
			const auto path = TypeOutputPath(sdkPath, record.m_Name);
			addPath(path, std::format("class {}::{}", record.m_Name.m_szModule, record.m_Name.m_szName));
			plan.m_ClassPaths.push_back(path);
		}
		return plan;
	}

	template <typename Function>
	void InSdkNamespace(codegen::CGenerator& generator, std::string_view module, Function&& function) {
		generator.Namespace("shade", [&] {
			generator.Namespace("sdk", [&] {
				if (module.empty())
					std::invoke(std::forward<Function>(function));
				else
					generator.Namespace(module, std::forward<Function>(function));
			});
		});
	}

	void GenerateCpp(const GeneratorConfig_t& config, const schema::CSchemaModel& model) {
		const codegen::CCppTypeFormatter formatter{ model };
		const auto                       plan = PlanCppOutput(config, model);
		{
			codegen::COutputFile cmakelists{ plan.m_CMakeListsPath };
			cmakelists << kszCMakeLists;
		}

		{
			const auto           dependencies = tools::BuildDependencies(model, model.GetAtomics());
			codegen::COutputFile outputFile{ plan.m_TypesPath };
			codegen::CGenerator  generator{ outputFile };
			codegen::CCppEmitter emitter{ generator, formatter };

			emitter.Prologue();
			InSdkNamespace(generator, {}, [&] { emitter.AtomicForwardDeclarations(model.GetAtomics()); });
			emitter.Dependencies(dependencies);
			InSdkNamespace(generator, {}, [&] {
				for (const auto& atomic : model.GetAtomics())
					emitter.Atomic(atomic);
			});
		}

		for (std::size_t index = 0; index < model.GetEnums().size(); ++index) {
			const auto&          record = model.GetEnums()[index];
			codegen::COutputFile outputFile{ plan.m_EnumPaths[index] };
			codegen::CGenerator  generator{ outputFile };
			codegen::CCppEmitter emitter{ generator, formatter };

			emitter.Prologue();
			InSdkNamespace(generator, record.m_Name.m_szModule, [&] { emitter.Enum(record); });
		}

		for (std::size_t index = 0; index < model.GetClasses().size(); ++index) {
			const auto&          record       = model.GetClasses()[index];
			const auto           dependencies = tools::BuildDependencies(model, record);
			codegen::COutputFile outputFile{ plan.m_ClassPaths[index] };
			codegen::CGenerator  generator{ outputFile };
			codegen::CCppEmitter emitter{ generator, formatter };

			emitter.Prologue();
			generator.Include("shade/sdk/types.hpp", codegen::EIncludeType::Local).NewLine();
			emitter.Dependencies(dependencies);
			InSdkNamespace(generator, record.m_Name.m_szModule, [&] { emitter.Class(record); });
		}

		lg::Info("generation", "C++ SDK is assembled at {}", plan.m_TypesPath.parent_path().string());
	}

	void MergeDeclarations(schema::SchemaDependencies_t& destination, const schema::SchemaDependencies_t& source) {
		for (const auto& [module, names] : source.m_Declarations) {
			auto& merged = destination.m_Declarations[module];
			merged.insert(names.begin(), names.end());
		}
	}

	void GenerateIda(const GeneratorConfig_t& config, const schema::CSchemaModel& model, codegen::EIdaSyntax syntax) {
		const auto classes = model.GetClasses();

		std::vector<schema::SchemaDependencies_t> dependencies;
		dependencies.reserve(classes.size());
		schema::SchemaDependencies_t forwardDeclarations;
		for (const auto& record : classes) {
			dependencies.push_back(tools::BuildDependencies(model, record));
			MergeDeclarations(forwardDeclarations, dependencies.back());
		}

		tools::DependencyKeySet_t enumKeys;
		enumKeys.reserve(model.GetEnums().size());
		for (const auto& record : model.GetEnums())
			enumKeys.emplace(record.m_Name);

		const auto indexMap = tools::BuildDependencyIndexMap(classes);
		auto       graph    = tools::BuildDependencyGraph(classes, dependencies, indexMap, enumKeys);
		const auto order    = tools::SortDependencyGraph(graph);
		if (order.size() != classes.size()) {
			lg::Error("generation", "IDA generation aborted: dependency sort returned {} of {} classes; no output file was written", order.size(),
			          classes.size());
			return;
		}

		const fs::path                   outputPath = config.m_OutputPath / "shade.hpp";
		codegen::COutputFile             outputFile{ outputPath };
		codegen::CGenerator              generator{ outputFile };
		const codegen::CIdaTypeFormatter formatter{ model };
		codegen::CIdaEmitter             emitter{ generator, formatter, syntax };

		emitter.Prologue();
		emitter.ForwardDeclarations(forwardDeclarations);
		for (const auto& record : model.GetAtomics())
			emitter.Atomic(record);
		for (const auto& record : model.GetEnums())
			emitter.Enum(record);
		for (const auto index : order)
			emitter.Class(classes[index]);

		lg::Info("generation", "IDA SDK is assembled at {}", outputPath.string());
	}

} // namespace

void shade::GenerateSdk(GeneratorConfig_t& config) {
	if (!g_pSchemaSystem)
		throw std::runtime_error("schema system is not available");

	tools::CSchemaCollector collector;
	schema::CSchemaModel    model = collector.Collect(*g_pSchemaSystem);
	if (model.GetClasses().empty() && model.GetEnums().empty() && model.GetAtomics().empty()) {
		lg::Error("generation", "zero schema records were collected. Are the modules loaded correctly?");
		return;
	}

	switch (config.m_eEmitType) {
	case EEmitType::CPP:
		GenerateCpp(config, model);
		break;
	case EEmitType::IDA_CPP:
		GenerateIda(config, model, codegen::EIdaSyntax::CPP);
		break;
	case EEmitType::IDA_C:
		GenerateIda(config, model, codegen::EIdaSyntax::C);
		break;
	default:
		throw std::runtime_error("unsupported SDK emit type");
	}
}
