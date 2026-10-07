#include "generation.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
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

	static constexpr std::string_view kszTypesFile  = "types.hpp";
	static constexpr std::string_view kszEnumsFile  = "enums.hpp";
	static constexpr std::string_view kszSingleFile = "shade.hpp";

	[[nodiscard]] std::string_view GetSplitModeName(ESplitMode mode) {
		switch (mode) {
		case ESplitMode::PER_FILE:
			return "per_file";
		case ESplitMode::MODULE:
			return "module";
		case ESplitMode::DUAL:
			return "dual";
		case ESplitMode::SINGLE:
			return "single";
		}
		throw std::runtime_error("unsupported split mode");
	}

	[[nodiscard]] std::string DescribeModule(std::string_view module) {
		return module.empty() ? std::string("<global>") : std::string(module);
	}

	[[nodiscard]] bool IsSafePathComponent(std::string_view component, bool allowEmpty, bool allowColon = false) {
		if (component.empty())
			return allowEmpty;
		if (component == "." || component == ".." || component.back() == ' ' || component.back() == '.')
			return false;

		const std::string_view forbidden = allowColon ? "<>\"/\\|?*" : "<>:\"/\\|?*";
		return component.find_first_of(forbidden) == std::string_view::npos && component.find('\0') == std::string_view::npos;
	}

	void ValidateModuleName(std::string_view module) {
		if (!IsSafePathComponent(module, true))
			throw std::runtime_error(std::format("unsafe schema module name for output path: {}", module));
	}

	void ValidateOutputName(const schema::SchemaTypeName_t& name) {
		ValidateModuleName(name.m_szModule);
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

	[[nodiscard]] std::string NormalizeOutputPathKey(const fs::path& path) {
		std::string key = path.lexically_normal().generic_string();
		std::ranges::transform(key, key.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return key;
	}

	/** @brief Reserves every destination of one run and rejects case-insensitive collisions before any file is opened. */
	class COutputPathRegistry {
	private:
		std::unordered_map<std::string, std::string> m_Owners;

	public:
		void Add(const fs::path& path, std::string owner) {
			const auto [position, inserted] = m_Owners.try_emplace(NormalizeOutputPathKey(path), owner);
			if (!inserted)
				throw std::runtime_error(std::format("output path collision between {} and {} at {}", position->second, owner, path.string()));
		}
	};

	/** @brief Destinations of a layout with one header per schema type. */
	struct PerTypeOutputPlan_t {
		std::vector<fs::path> m_EnumPaths; ///< Parallel to model enums; empty when enums are written to a shared file.
		std::vector<fs::path> m_ClassPaths; ///< Parallel to model classes.
	};

	[[nodiscard]] PerTypeOutputPlan_t PlanPerTypeOutput(const fs::path& sdkPath, const schema::CSchemaModel& model, bool includeEnums,
	                                                    COutputPathRegistry& registry) {
		PerTypeOutputPlan_t plan;
		if (includeEnums) {
			plan.m_EnumPaths.reserve(model.GetEnums().size());
			for (const auto& record : model.GetEnums()) {
				auto path = TypeOutputPath(sdkPath, record.m_Name);
				registry.Add(path, std::format("enum {}::{}", record.m_Name.m_szModule, record.m_Name.m_szName));
				plan.m_EnumPaths.push_back(std::move(path));
			}
		}

		plan.m_ClassPaths.reserve(model.GetClasses().size());
		for (const auto& record : model.GetClasses()) {
			auto path = TypeOutputPath(sdkPath, record.m_Name);
			registry.Add(path, std::format("class {}::{}", record.m_Name.m_szModule, record.m_Name.m_szName));
			plan.m_ClassPaths.push_back(std::move(path));
		}
		return plan;
	}

	/** @brief Complete-definition relations between model records and an order that defines every class after its requirements. */
	struct ClassGraph_t {
		std::vector<schema::SchemaDependencies_t> m_Dependencies; ///< Per class, in model order.
		std::vector<std::vector<std::size_t>>     m_RequiredClasses; ///< Class definitions each class needs.
		std::vector<std::vector<std::size_t>>     m_RequiredEnums; ///< Enum definitions each class needs.
		std::vector<std::size_t>                  m_Order; ///< Class indices in definition order.
		std::vector<std::size_t>                  m_OrderPositions; ///< Position of each class in `m_Order`.
		tools::DependencyIndexMap_t               m_ClassIndices;
	};

	[[nodiscard]] ClassGraph_t BuildClassGraph(const schema::CSchemaModel& model, tools::EAtomicParameters atomicParameters) {
		const auto classes = model.GetClasses();
		const auto enums   = model.GetEnums();

		ClassGraph_t graph;
		graph.m_Dependencies.reserve(classes.size());
		for (const auto& record : classes)
			graph.m_Dependencies.push_back(tools::BuildDependencies(model, record, atomicParameters));

		std::unordered_map<schema::SchemaTypeName_t, std::size_t, tools::SchemaTypeNameHash_t> enumIndices;
		tools::DependencyKeySet_t                                                              enumKeys;
		enumIndices.reserve(enums.size());
		enumKeys.reserve(enums.size());
		for (std::size_t index = 0; index < enums.size(); ++index) {
			enumIndices.emplace(enums[index].m_Name, index);
			enumKeys.emplace(enums[index].m_Name);
		}
		graph.m_ClassIndices = tools::BuildDependencyIndexMap(classes);

		graph.m_RequiredClasses.resize(classes.size());
		graph.m_RequiredEnums.resize(classes.size());
		for (std::size_t index = 0; index < classes.size(); ++index) {
			for (const auto& [module, names] : graph.m_Dependencies[index].m_Definitions) {
				for (const auto& name : names) {
					const schema::SchemaTypeName_t key{ .m_szModule = module, .m_szName = name };
					if (const auto classIndex = graph.m_ClassIndices.GetIndex(key); classIndex != tools::DependencyIndexMap_t::InvalidIndex())
						graph.m_RequiredClasses[index].push_back(classIndex);
					else if (const auto enumIndex = enumIndices.find(key); enumIndex != enumIndices.end())
						graph.m_RequiredEnums[index].push_back(enumIndex->second);
					else
						throw std::runtime_error(std::format("{}::{} requires an uncollected definition {}::{}", classes[index].m_Name.m_szModule,
						                                     classes[index].m_Name.m_szName, module, name));
				}
			}
			std::ranges::sort(graph.m_RequiredClasses[index]);
			std::ranges::sort(graph.m_RequiredEnums[index]);
		}

		auto nodes    = tools::BuildDependencyGraph(classes, graph.m_Dependencies, graph.m_ClassIndices, enumKeys);
		graph.m_Order = tools::SortDependencyGraph(nodes);
		if (graph.m_Order.size() != classes.size())
			throw std::runtime_error("cannot order cyclic class definitions");

		graph.m_OrderPositions.resize(classes.size());
		for (std::size_t position = 0; position < graph.m_Order.size(); ++position)
			graph.m_OrderPositions[graph.m_Order[position]] = position;
		return graph;
	}

	/** @brief Returns every class whose definition must precede a class, in definition order. */
	[[nodiscard]] std::vector<std::size_t> CollectRequiredClosure(const ClassGraph_t& graph, std::size_t classIndex) {
		std::vector<std::size_t>        closure;
		std::unordered_set<std::size_t> visited;
		std::vector<std::size_t>        pending(graph.m_RequiredClasses[classIndex].begin(), graph.m_RequiredClasses[classIndex].end());
		while (!pending.empty()) {
			const auto current = pending.back();
			pending.pop_back();
			if (!visited.insert(current).second)
				continue;

			closure.push_back(current);
			pending.insert(pending.end(), graph.m_RequiredClasses[current].begin(), graph.m_RequiredClasses[current].end());
		}
		std::ranges::sort(closure, {}, [&](std::size_t index) { return graph.m_OrderPositions[index]; });
		return closure;
	}

	/** @brief One header of a module layout: a module header or a shared header that breaks a module-header cycle. */
	struct ModuleLayoutFile_t {
		fs::path                     m_Path;
		std::vector<std::size_t>     m_EnumIndices; ///< Complete enum definitions.
		std::vector<std::size_t>     m_OpaqueEnumIndices; ///< Enums whose module header follows this file and must be declared opaquely.
		std::vector<std::size_t>     m_ClassIndices; ///< Class definitions in definition order.
		std::vector<std::size_t>     m_RequiredFiles; ///< Earlier files whose definitions this file needs directly.
		std::vector<std::size_t>     m_PrerequisiteFiles; ///< Transitive closure of `m_RequiredFiles`, in file order.
		schema::SchemaDependencies_t m_Declarations; ///< Referenced types that no prerequisite file defines.
	};

	/** @brief Files of a module layout; every file follows all of its prerequisites. */
	struct ModuleLayoutPlan_t {
		std::vector<ModuleLayoutFile_t> m_Files;
	};

	struct ModuleLayoutOptions_t {
		bool m_bNestedHeaders; ///< Place each module header inside its module directory, as the dual layout does.
		bool m_bEnumsInModules; ///< Define enums in module headers; otherwise they live in a shared file that precedes all module headers.
	};

	/**
	 * @brief Plans one header per schema module and resolves module-header cycles.
	 *
	 * Modules that need complete definitions from each other form a cycle. Its
	 * members are ordered so that earlier ones need the fewest classes from the
	 * rest; every class that an earlier member needs from a later one moves,
	 * with its definition closure inside the cycle, to `__cycle_<n>.hpp`, which
	 * precedes all members. An enum that is needed before its module header is
	 * declared opaquely instead.
	 */
	[[nodiscard]] ModuleLayoutPlan_t PlanModuleLayout(const schema::CSchemaModel& model, const ClassGraph_t& graph, const fs::path& sdkPath,
	                                                  const ModuleLayoutOptions_t& options, COutputPathRegistry& registry) {
		const auto classes = model.GetClasses();
		const auto enums   = model.GetEnums();

		std::map<std::string, std::size_t> moduleIndices;
		for (const auto& record : classes)
			moduleIndices.try_emplace(record.m_Name.m_szModule, 0);
		if (options.m_bEnumsInModules)
			for (const auto& record : enums)
				moduleIndices.try_emplace(record.m_Name.m_szModule, 0);

		std::vector<std::string_view> moduleNames;
		moduleNames.reserve(moduleIndices.size());
		for (auto& [module, index] : moduleIndices) {
			ValidateModuleName(module);
			index = moduleNames.size();
			moduleNames.push_back(module);
		}
		const auto moduleCount = moduleNames.size();
		const auto noModule    = moduleCount;

		std::vector<std::size_t>              classModules(classes.size());
		std::vector<std::vector<std::size_t>> moduleClasses(moduleCount);
		for (std::size_t index = 0; index < classes.size(); ++index)
			classModules[index] = moduleIndices.at(classes[index].m_Name.m_szModule);
		for (const auto index : graph.m_Order)
			moduleClasses[classModules[index]].push_back(index);

		std::vector<std::size_t>              enumModules(enums.size(), noModule);
		std::vector<std::vector<std::size_t>> moduleEnums(moduleCount);
		if (options.m_bEnumsInModules) {
			for (std::size_t index = 0; index < enums.size(); ++index) {
				enumModules[index] = moduleIndices.at(enums[index].m_Name.m_szModule);
				moduleEnums[enumModules[index]].push_back(index);
			}
		}

		std::vector<std::vector<bool>> requiredModules(moduleCount, std::vector<bool>(moduleCount));
		for (std::size_t index = 0; index < classes.size(); ++index) {
			auto& required = requiredModules[classModules[index]];
			for (const auto dependency : graph.m_RequiredClasses[index])
				required[classModules[dependency]] = true;
			for (const auto dependency : graph.m_RequiredEnums[index])
				if (enumModules[dependency] != noModule)
					required[enumModules[dependency]] = true;
		}

		std::vector<std::vector<bool>> reachable(moduleCount, std::vector<bool>(moduleCount));
		for (std::size_t start = 0; start < moduleCount; ++start) {
			std::vector<std::size_t> pending{ start };
			while (!pending.empty()) {
				const auto current = pending.back();
				pending.pop_back();
				if (reachable[start][current])
					continue;

				reachable[start][current] = true;
				for (std::size_t next = 0; next < moduleCount; ++next)
					if (requiredModules[current][next])
						pending.push_back(next);
			}
		}

		std::vector<std::size_t>              componentOf(moduleCount, moduleCount);
		std::vector<std::vector<std::size_t>> components;
		for (std::size_t first = 0; first < moduleCount; ++first) {
			if (componentOf[first] != moduleCount)
				continue;

			std::vector<std::size_t> members;
			for (std::size_t member = first; member < moduleCount; ++member) {
				if (reachable[first][member] && reachable[member][first]) {
					componentOf[member] = components.size();
					members.push_back(member);
				}
			}
			components.push_back(std::move(members));
		}

		std::vector<std::size_t> componentOrder;
		std::vector<bool>        ordered(components.size());
		const auto               isReady = [&](std::size_t component) {
			return std::ranges::all_of(components[component], [&](std::size_t member) {
				for (std::size_t target = 0; target < moduleCount; ++target)
					if (requiredModules[member][target] && componentOf[target] != component && !ordered[componentOf[target]])
						return false;
				return true;
			});
		};
		while (componentOrder.size() < components.size()) {
			std::size_t next = 0;
			while (next < components.size() && (ordered[next] || !isReady(next)))
				++next;
			if (next == components.size())
				throw std::runtime_error("module layout cannot order module headers");

			ordered[next] = true;
			componentOrder.push_back(next);
		}

		ModuleLayoutPlan_t       plan;
		std::vector<std::size_t> moduleFiles(moduleCount);
		std::vector<std::size_t> classFiles(classes.size());
		std::size_t              cycleCount = 0;

		auto addFile = [&](fs::path path, std::string owner) {
			registry.Add(path, std::move(owner));
			plan.m_Files.emplace_back().m_Path = std::move(path);
			return plan.m_Files.size() - 1;
		};
		auto addModuleFile = [&](std::size_t module, std::vector<std::size_t> classIndices, std::optional<std::size_t> cycleFile) {
			const std::string name = std::string(moduleNames[module]);
			const fs::path    path = name.empty()             ? sdkPath / "__global.hpp" :
			                         options.m_bNestedHeaders ? sdkPath / name / (name + ".hpp") :
			                                                    sdkPath / (name + ".hpp");

			const auto fileIndex = addFile(path, std::format("module {}", DescribeModule(name)));
			auto&      file      = plan.m_Files[fileIndex];
			file.m_EnumIndices   = moduleEnums[module];
			file.m_ClassIndices  = std::move(classIndices);
			if (cycleFile)
				file.m_RequiredFiles.push_back(*cycleFile);
			for (const auto index : file.m_ClassIndices)
				classFiles[index] = fileIndex;
			moduleFiles[module] = fileIndex;
		};

		for (const auto component : componentOrder) {
			auto members = components[component];
			if (members.size() == 1) {
				addModuleFile(members.front(), moduleClasses[members.front()], std::nullopt);
				continue;
			}

			std::vector<std::size_t> crossDefinitionCounts(moduleCount);
			for (const auto member : members) {
				std::unordered_set<std::size_t> targets;
				for (const auto index : moduleClasses[member])
					for (const auto dependency : graph.m_RequiredClasses[index])
						if (const auto target = classModules[dependency]; target != member && componentOf[target] == component)
							targets.insert(dependency);
				crossDefinitionCounts[member] = targets.size();
			}
			std::ranges::stable_sort(members, {}, [&](std::size_t member) { return crossDefinitionCounts[member]; });

			std::vector<std::size_t> positions(moduleCount);
			for (std::size_t position = 0; position < members.size(); ++position)
				positions[members[position]] = position;

			std::vector<bool>        extracted(classes.size());
			std::vector<std::size_t> pending;
			auto                     extract = [&](std::size_t index) {
				if (!extracted[index]) {
					extracted[index] = true;
					pending.push_back(index);
				}
			};
			for (const auto member : members)
				for (const auto index : moduleClasses[member])
					for (const auto dependency : graph.m_RequiredClasses[index])
						if (const auto target = classModules[dependency]; componentOf[target] == component && positions[target] > positions[member])
							extract(dependency);
			while (!pending.empty()) {
				const auto current = pending.back();
				pending.pop_back();
				for (const auto dependency : graph.m_RequiredClasses[current])
					if (componentOf[classModules[dependency]] == component)
						extract(dependency);
			}

			std::optional<std::size_t> cycleFile;
			std::vector<std::size_t>   shared;
			for (const auto index : graph.m_Order)
				if (extracted[index])
					shared.push_back(index);
			if (!shared.empty()) {
				std::string memberNames;
				for (const auto member : members)
					memberNames += (memberNames.empty() ? "" : ", ") + DescribeModule(moduleNames[member]);

				const auto fileIndex =
				    addFile(sdkPath / std::format("__cycle_{}.hpp", cycleCount), std::format("module cycle {} ({})", cycleCount, memberNames));
				++cycleCount;
				for (const auto index : shared)
					classFiles[index] = fileIndex;
				plan.m_Files[fileIndex].m_ClassIndices = std::move(shared);
				cycleFile                              = fileIndex;
			}

			for (const auto member : members) {
				std::vector<std::size_t> remaining;
				for (const auto index : moduleClasses[member])
					if (!extracted[index])
						remaining.push_back(index);
				addModuleFile(member, std::move(remaining), cycleFile);
			}
		}

		const codegen::CCppTypeFormatter enumFormatter{ model };
		for (std::size_t fileIndex = 0; fileIndex < plan.m_Files.size(); ++fileIndex) {
			auto&                 file = plan.m_Files[fileIndex];
			std::set<std::size_t> required(file.m_RequiredFiles.begin(), file.m_RequiredFiles.end());
			std::set<std::size_t> opaqueEnums;
			for (const auto index : file.m_ClassIndices) {
				for (const auto dependency : graph.m_RequiredClasses[index]) {
					const auto target = classFiles[dependency];
					if (target > fileIndex)
						throw std::runtime_error(std::format("module layout places {}::{} after its dependent {}::{}",
						                                     classes[dependency].m_Name.m_szModule, classes[dependency].m_Name.m_szName,
						                                     classes[index].m_Name.m_szModule, classes[index].m_Name.m_szName));
					if (target != fileIndex)
						required.insert(target);
				}

				for (const auto dependency : graph.m_RequiredEnums[index]) {
					if (enumModules[dependency] == noModule)
						continue;

					const auto target = moduleFiles[enumModules[dependency]];
					if (target < fileIndex)
						required.insert(target);
					else if (target > fileIndex)
						opaqueEnums.insert(dependency);
				}
			}

			for (const auto index : opaqueEnums)
				if (!enumFormatter.IsValidEnumUnderlyingType(enums[index].m_UnderlyingType))
					throw std::runtime_error(
					    std::format("module layout cannot forward-declare enum {}::{}", enums[index].m_Name.m_szModule, enums[index].m_Name.m_szName));

			std::vector<bool> prerequisites(plan.m_Files.size());
			for (const auto target : required) {
				prerequisites[target] = true;
				for (const auto transitive : plan.m_Files[target].m_PrerequisiteFiles)
					prerequisites[transitive] = true;
			}

			file.m_RequiredFiles.assign(required.begin(), required.end());
			file.m_OpaqueEnumIndices.assign(opaqueEnums.begin(), opaqueEnums.end());
			for (std::size_t target = 0; target < fileIndex; ++target)
				if (prerequisites[target])
					file.m_PrerequisiteFiles.push_back(target);

			for (const auto index : file.m_ClassIndices)
				for (const auto& [module, names] : graph.m_Dependencies[index].m_Declarations)
					for (const auto& name : names) {
						const auto declared = graph.m_ClassIndices.GetIndex({ .m_szModule = module, .m_szName = name });
						if (declared == tools::DependencyIndexMap_t::InvalidIndex() || !prerequisites[classFiles[declared]])
							file.m_Declarations.m_Declarations[module].insert(name);
					}
		}
		return plan;
	}

	/** @brief Declarations that let aggregate C++ layouts define atomics before any module or single header. */
	struct CppTypesPlan_t {
		schema::SchemaDependencies_t m_Declarations; ///< Classes used as atomic template parameters.
		std::vector<std::size_t>     m_OpaqueEnumIndices; ///< Enums used as atomic template parameters.
	};

	[[nodiscard]] CppTypesPlan_t PlanCppTypes(const schema::CSchemaModel& model, const codegen::CCppTypeFormatter& formatter) {
		const auto     enums        = model.GetEnums();
		const auto     dependencies = tools::BuildDependencies(model, model.GetAtomics());
		CppTypesPlan_t plan{ .m_Declarations = { .m_Definitions = {}, .m_Declarations = dependencies.m_Declarations }, .m_OpaqueEnumIndices = {} };
		for (const auto& [module, names] : tools::GetSortedDependencyGroups(dependencies.m_Definitions))
			for (const auto name : names) {
				const auto position =
				    std::ranges::find_if(enums, [&](const auto& record) { return record.m_Name.m_szModule == module && record.m_Name.m_szName == name; });
				if (position == enums.end() || !formatter.IsValidEnumUnderlyingType(position->m_UnderlyingType))
					throw std::runtime_error(std::format("types.hpp cannot forward-declare enum {}::{}", module, name));
				plan.m_OpaqueEnumIndices.push_back(static_cast<std::size_t>(position - enums.begin()));
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

	/** @brief Emits records in their given order, opening one SDK namespace block per run of records from the same module. */
	template <typename ModuleOf, typename Function>
	void InSdkNamespaceRuns(codegen::CGenerator& generator, std::span<const std::size_t> indices, ModuleOf&& moduleOf, bool separateRecords,
	                        Function&& emit) {
		for (std::size_t begin = 0; begin < indices.size();) {
			const std::string_view module = moduleOf(indices[begin]);
			std::size_t            end    = begin + 1;
			while (end < indices.size() && moduleOf(indices[end]) == module)
				++end;

			InSdkNamespace(generator, module, [&] {
				for (std::size_t position = begin; position < end; ++position) {
					if (separateRecords && position != begin)
						generator.NewLine();
					emit(indices[position]);
				}
			});
			generator.NewLine();
			begin = end;
		}
	}

	void EmitClassForwardDeclaration(codegen::CGenerator& generator, const schema::SchemaClassRecord_t& record) {
		if (record.m_eType == schema::ESchemaClassType::STRUCT)
			generator.StructForwardDeclaration(record.m_Name.m_szName);
		else
			generator.ClassForwardDeclaration(record.m_Name.m_szName);
	}

	void WriteCppCMakeLists(const fs::path& rootPath) {
		codegen::COutputFile cmakelists{ rootPath / "CMakeLists.txt" };
		cmakelists << kszCMakeLists;
	}

	/** @brief Writes types.hpp for the per-type layout, which includes the enum headers used by atomic specializations. */
	void WritePerTypeCppTypes(const fs::path& typesPath, const schema::CSchemaModel& model, const codegen::CCppTypeFormatter& formatter) {
		const auto           dependencies = tools::BuildDependencies(model, model.GetAtomics());
		codegen::COutputFile outputFile{ typesPath };
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

	/** @brief Writes types.hpp for aggregate layouts, which declare enum parameters opaquely instead of including module headers. */
	void WriteCppTypes(const fs::path& typesPath, const CppTypesPlan_t& plan, const schema::CSchemaModel& model,
	                   const codegen::CCppTypeFormatter& formatter) {
		codegen::COutputFile outputFile{ typesPath };
		codegen::CGenerator  generator{ outputFile };
		codegen::CCppEmitter emitter{ generator, formatter };

		emitter.Prologue();
		InSdkNamespace(generator, {}, [&] { emitter.AtomicForwardDeclarations(model.GetAtomics()); });
		emitter.ForwardDeclarations(plan.m_Declarations);
		for (const auto index : plan.m_OpaqueEnumIndices) {
			const auto& record = model.GetEnums()[index];
			InSdkNamespace(generator, record.m_Name.m_szModule,
			               [&] { generator.EnumForwardDeclaration(record.m_Name.m_szName, formatter.FormatType(record.m_UnderlyingType)); });
		}
		InSdkNamespace(generator, {}, [&] {
			for (const auto& atomic : model.GetAtomics())
				emitter.Atomic(atomic);
		});
	}

	void WritePerTypeCppHeaders(const PerTypeOutputPlan_t& plan, const schema::CSchemaModel& model, const codegen::CCppTypeFormatter& formatter) {
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
	}

	void WriteCppModuleLayout(const ModuleLayoutPlan_t& plan, const fs::path& rootPath, const schema::CSchemaModel& model,
	                          const codegen::CCppTypeFormatter& formatter) {
		const auto     classes     = model.GetClasses();
		const auto     enums       = model.GetEnums();
		const fs::path includeRoot = rootPath.filename();
		const auto     classModule = [&](std::size_t index) -> std::string_view {
			return classes[index].m_Name.m_szModule;
		};
		const auto enumModule = [&](std::size_t index) -> std::string_view {
			return enums[index].m_Name.m_szModule;
		};

		for (const auto& file : plan.m_Files) {
			std::vector<std::string> includes{ (includeRoot / "sdk" / kszTypesFile).generic_string() };
			for (const auto required : file.m_RequiredFiles)
				includes.push_back((includeRoot / plan.m_Files[required].m_Path.lexically_relative(rootPath)).generic_string());

			// Classes defined here are declared below with their exact class keys.
			schema::SchemaDependencies_t declarations = file.m_Declarations;
			for (const auto index : file.m_ClassIndices)
				if (const auto group = declarations.m_Declarations.find(classes[index].m_Name.m_szModule); group != declarations.m_Declarations.end())
					group->second.erase(classes[index].m_Name.m_szName);
			std::erase_if(declarations.m_Declarations, [](const auto& entry) { return entry.second.empty(); });

			auto declaredClasses = file.m_ClassIndices;
			std::ranges::stable_sort(declaredClasses, {}, classModule);
			auto opaqueEnums = file.m_OpaqueEnumIndices;
			std::ranges::stable_sort(opaqueEnums, {}, enumModule);

			codegen::COutputFile outputFile{ file.m_Path };
			codegen::CGenerator  generator{ outputFile };
			codegen::CCppEmitter emitter{ generator, formatter };

			emitter.Prologue();
			emitter.Includes(includes);
			emitter.ForwardDeclarations(declarations);
			InSdkNamespaceRuns(generator, declaredClasses, classModule, false,
			                   [&](std::size_t index) { EmitClassForwardDeclaration(generator, classes[index]); });
			InSdkNamespaceRuns(generator, opaqueEnums, enumModule, false, [&](std::size_t index) {
				generator.EnumForwardDeclaration(enums[index].m_Name.m_szName, formatter.FormatType(enums[index].m_UnderlyingType));
			});
			InSdkNamespaceRuns(generator, file.m_EnumIndices, enumModule, true, [&](std::size_t index) { emitter.Enum(enums[index]); });
			InSdkNamespaceRuns(generator, file.m_ClassIndices, classModule, true, [&](std::size_t index) { emitter.Class(classes[index]); });
		}
	}

	void WriteCppSingle(const fs::path& path, const fs::path& rootPath, const ClassGraph_t& graph, const schema::CSchemaModel& model,
	                    const codegen::CCppTypeFormatter& formatter) {
		const auto classes     = model.GetClasses();
		const auto enums       = model.GetEnums();
		const auto classModule = [&](std::size_t index) -> std::string_view {
			return classes[index].m_Name.m_szModule;
		};
		const auto enumModule = [&](std::size_t index) -> std::string_view {
			return enums[index].m_Name.m_szModule;
		};

		// Every collected class is declared below; only references to uncollected classes need generic declarations.
		schema::SchemaDependencies_t declarations;
		for (const auto& dependencies : graph.m_Dependencies)
			for (const auto& [module, names] : dependencies.m_Declarations)
				for (const auto& name : names)
					if (graph.m_ClassIndices.GetIndex({ .m_szModule = module, .m_szName = name }) == tools::DependencyIndexMap_t::InvalidIndex())
						declarations.m_Declarations[module].insert(name);

		std::vector<std::size_t> declaredClasses(classes.size());
		std::iota(declaredClasses.begin(), declaredClasses.end(), std::size_t{ 0 });
		std::ranges::stable_sort(declaredClasses, {}, classModule);
		std::vector<std::size_t> enumOrder(enums.size());
		std::iota(enumOrder.begin(), enumOrder.end(), std::size_t{ 0 });
		std::ranges::stable_sort(enumOrder, {}, enumModule);

		codegen::COutputFile outputFile{ path };
		codegen::CGenerator  generator{ outputFile };
		codegen::CCppEmitter emitter{ generator, formatter };

		emitter.Prologue();
		generator.Include((rootPath.filename() / "sdk" / kszTypesFile).generic_string(), codegen::EIncludeType::Local).NewLine();
		emitter.ForwardDeclarations(declarations);
		InSdkNamespaceRuns(generator, declaredClasses, classModule, false,
		                   [&](std::size_t index) { EmitClassForwardDeclaration(generator, classes[index]); });
		InSdkNamespaceRuns(generator, enumOrder, enumModule, true, [&](std::size_t index) { emitter.Enum(enums[index]); });
		InSdkNamespaceRuns(generator, graph.m_Order, classModule, true, [&](std::size_t index) { emitter.Class(classes[index]); });
	}

	void GenerateCpp(const fs::path& rootPath, ESplitMode mode, const schema::CSchemaModel& model) {
		const fs::path                   sdkPath   = rootPath / "sdk";
		const fs::path                   typesPath = sdkPath / kszTypesFile;
		const codegen::CCppTypeFormatter formatter{ model };

		COutputPathRegistry registry;
		registry.Add(typesPath, "reserved types.hpp");

		switch (mode) {
		case ESplitMode::PER_FILE: {
			const auto plan = PlanPerTypeOutput(sdkPath, model, true, registry);
			WriteCppCMakeLists(rootPath);
			WritePerTypeCppTypes(typesPath, model, formatter);
			WritePerTypeCppHeaders(plan, model, formatter);
			break;
		}
		case ESplitMode::MODULE:
		case ESplitMode::DUAL: {
			const bool dual        = mode == ESplitMode::DUAL;
			const auto graph       = BuildClassGraph(model, tools::EAtomicParameters::Declare);
			const auto modulePlan  = PlanModuleLayout(model, graph, sdkPath, { .m_bNestedHeaders = dual, .m_bEnumsInModules = true }, registry);
			const auto perTypePlan = dual ? std::optional{ PlanPerTypeOutput(sdkPath, model, true, registry) } : std::nullopt;
			const auto typesPlan   = PlanCppTypes(model, formatter);

			WriteCppCMakeLists(rootPath);
			WriteCppTypes(typesPath, typesPlan, model, formatter);
			WriteCppModuleLayout(modulePlan, rootPath, model, formatter);
			if (perTypePlan)
				WritePerTypeCppHeaders(*perTypePlan, model, formatter);
			break;
		}
		case ESplitMode::SINGLE: {
			const fs::path singlePath = rootPath / kszSingleFile;
			registry.Add(singlePath, "single SDK header");
			const auto graph     = BuildClassGraph(model, tools::EAtomicParameters::Declare);
			const auto typesPlan = PlanCppTypes(model, formatter);

			WriteCppCMakeLists(rootPath);
			WriteCppTypes(typesPath, typesPlan, model, formatter);
			WriteCppSingle(singlePath, rootPath, graph, model, formatter);
			break;
		}
		default:
			throw std::runtime_error("unsupported split mode");
		}

		lg::Info("generation", "C++ SDK ({} layout) is assembled at {}", GetSplitModeName(mode), rootPath.string());
	}

	void MergeDeclarations(schema::SchemaDependencies_t& destination, const schema::SchemaDependencies_t& source) {
		for (const auto& [module, names] : source.m_Declarations) {
			auto& merged = destination.m_Declarations[module];
			merged.insert(names.begin(), names.end());
		}
	}

	void WriteIdaSingle(const fs::path& outputPath, const schema::CSchemaModel& model, codegen::EIdaSyntax syntax) {
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
	}

	/** @brief Writes the IDA headers that every split IDA header requires first; neither depends on a class. */
	void WriteIdaSharedFiles(const fs::path& sdkPath, const schema::CSchemaModel& model, const codegen::CIdaTypeFormatter& formatter,
	                         codegen::EIdaSyntax syntax) {
		{
			codegen::COutputFile outputFile{ sdkPath / kszTypesFile };
			codegen::CGenerator  generator{ outputFile };
			codegen::CIdaEmitter emitter{ generator, formatter, syntax };
			emitter.Prologue();
			for (const auto& record : model.GetAtomics())
				emitter.Atomic(record);
		}

		codegen::COutputFile outputFile{ sdkPath / kszEnumsFile };
		codegen::CGenerator  generator{ outputFile };
		codegen::CIdaEmitter emitter{ generator, formatter, syntax };
		emitter.Prologue();
		for (const auto& record : model.GetEnums())
			emitter.Enum(record);
	}

	/** @brief Returns the import list of a split IDA header, starting with the shared atomic and enum headers. */
	[[nodiscard]] std::vector<std::string> IdaImportRequirements(const fs::path& rootPath, std::span<const fs::path> prerequisites) {
		std::vector<std::string> requirements{ (fs::path{ "sdk" } / kszTypesFile).generic_string(), (fs::path{ "sdk" } / kszEnumsFile).generic_string() };
		requirements.reserve(requirements.size() + prerequisites.size());
		for (const auto& path : prerequisites)
			requirements.push_back(path.lexically_relative(rootPath).generic_string());
		return requirements;
	}

	void WriteIdaHeader(const fs::path& path, std::span<const std::string> requirements, const schema::SchemaDependencies_t& declarations,
	                    std::span<const std::size_t> classIndices, const schema::CSchemaModel& model, const codegen::CIdaTypeFormatter& formatter,
	                    codegen::EIdaSyntax syntax) {
		codegen::COutputFile outputFile{ path };
		codegen::CGenerator  generator{ outputFile };
		codegen::CIdaEmitter emitter{ generator, formatter, syntax };

		emitter.Prologue();
		emitter.ImportRequirements(requirements);
		emitter.ForwardDeclarations(declarations);
		for (const auto index : classIndices)
			emitter.Class(model.GetClasses()[index]);
	}

	void WriteIdaPerType(const PerTypeOutputPlan_t& plan, const ClassGraph_t& graph, const fs::path& rootPath, const schema::CSchemaModel& model,
	                     const codegen::CIdaTypeFormatter& formatter, codegen::EIdaSyntax syntax) {
		for (std::size_t index = 0; index < model.GetClasses().size(); ++index) {
			const auto                            closure = CollectRequiredClosure(graph, index);
			const std::unordered_set<std::size_t> imported(closure.begin(), closure.end());

			std::vector<fs::path> prerequisites;
			prerequisites.reserve(closure.size());
			for (const auto dependency : closure)
				prerequisites.push_back(plan.m_ClassPaths[dependency]);

			// Pointer targets defined by an earlier import already exist in the IDA database.
			schema::SchemaDependencies_t declarations;
			for (const auto& [module, names] : graph.m_Dependencies[index].m_Declarations)
				for (const auto& name : names) {
					const auto declared = graph.m_ClassIndices.GetIndex({ .m_szModule = module, .m_szName = name });
					if (declared == tools::DependencyIndexMap_t::InvalidIndex() || !imported.contains(declared))
						declarations.m_Declarations[module].insert(name);
				}

			WriteIdaHeader(plan.m_ClassPaths[index], IdaImportRequirements(rootPath, prerequisites), declarations, std::span{ &index, 1 }, model,
			               formatter, syntax);
		}
	}

	void WriteIdaModuleLayout(const ModuleLayoutPlan_t& plan, const fs::path& rootPath, const schema::CSchemaModel& model,
	                          const codegen::CIdaTypeFormatter& formatter, codegen::EIdaSyntax syntax) {
		for (const auto& file : plan.m_Files) {
			std::vector<fs::path> prerequisites;
			prerequisites.reserve(file.m_PrerequisiteFiles.size());
			for (const auto prerequisite : file.m_PrerequisiteFiles)
				prerequisites.push_back(plan.m_Files[prerequisite].m_Path);

			WriteIdaHeader(file.m_Path, IdaImportRequirements(rootPath, prerequisites), file.m_Declarations, file.m_ClassIndices, model, formatter,
			               syntax);
		}
	}

	void GenerateIda(const fs::path& rootPath, ESplitMode mode, const schema::CSchemaModel& model, codegen::EIdaSyntax syntax) {
		if (mode == ESplitMode::SINGLE) {
			const fs::path singlePath = rootPath / kszSingleFile;
			WriteIdaSingle(singlePath, model, syntax);
			lg::Info("generation", "IDA SDK (single layout) is assembled at {}", singlePath.string());
			return;
		}

		const fs::path      sdkPath = rootPath / "sdk";
		COutputPathRegistry registry;
		registry.Add(sdkPath / kszTypesFile, "reserved types.hpp");
		registry.Add(sdkPath / kszEnumsFile, "reserved enums.hpp");

		const bool perType = mode == ESplitMode::PER_FILE || mode == ESplitMode::DUAL;
		const bool modules = mode == ESplitMode::MODULE || mode == ESplitMode::DUAL;
		if (!perType && !modules)
			throw std::runtime_error("unsupported split mode");

		// IDA names atomic specializations without referencing their parameters, so those parameters need no declarations.
		const auto graph       = BuildClassGraph(model, tools::EAtomicParameters::Ignore);
		const auto perTypePlan = perType ? std::optional{ PlanPerTypeOutput(sdkPath, model, false, registry) } : std::nullopt;
		const auto modulePlan =
		    modules ? std::optional{ PlanModuleLayout(model, graph, sdkPath, { .m_bNestedHeaders = perType, .m_bEnumsInModules = false }, registry) } :
		              std::nullopt;

		const codegen::CIdaTypeFormatter formatter{ model };
		WriteIdaSharedFiles(sdkPath, model, formatter, syntax);
		if (modulePlan)
			WriteIdaModuleLayout(*modulePlan, rootPath, model, formatter, syntax);
		if (perTypePlan)
			WriteIdaPerType(*perTypePlan, graph, rootPath, model, formatter, syntax);

		lg::Info("generation", "IDA SDK ({} layout) is assembled at {}", GetSplitModeName(mode), rootPath.string());
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
		GenerateCpp(config.m_OutputPath / "shade", config.m_eSplitMode, model);
		break;
	case EEmitType::IDA_CPP:
		GenerateIda(config.m_OutputPath / "shade_ida", config.m_eSplitMode, model, codegen::EIdaSyntax::CPP);
		break;
	case EEmitType::IDA_C:
		GenerateIda(config.m_OutputPath / "shade_ida", config.m_eSplitMode, model, codegen::EIdaSyntax::C);
		break;
	default:
		throw std::runtime_error("unsupported SDK emit type");
	}
}
