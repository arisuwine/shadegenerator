#include "sort.hpp"

#include <algorithm>
#include <queue>
#include <utility>

#include "utils/debug.hpp"

using namespace shade;

std::vector<tools::SortedDependencyGroup_t> tools::GetSortedDependencyGroups(const schema::SchemaDependencyMap_t& dependencies) {
	std::vector<SortedDependencyGroup_t> result;
	result.reserve(dependencies.size());

	for (const auto& [module, names] : dependencies) {
		auto& group = result.emplace_back(SortedDependencyGroup_t{ .m_szModule = module, .m_Names = {} });
		group.m_Names.reserve(names.size());
		for (const auto& name : names)
			group.m_Names.emplace_back(name);

		std::ranges::sort(group.m_Names);
	}

	std::ranges::sort(result, {}, &SortedDependencyGroup_t::m_szModule);
	return result;
}

tools::DependencyIndexMap_t tools::BuildDependencyIndexMap(std::span<const schema::SchemaClassRecord_t> records) {
	DependencyIndexMap_t result;
	for (std::size_t index = 0; index < records.size(); ++index) {
		if (!result.Add(records[index].m_Name, index))
			lg::Warn("dependency", "duplicate class index, module {} name {}", records[index].m_Name.m_szModule, records[index].m_Name.m_szName);
	}

	return result;
}

std::vector<tools::CDependencyGraphNode> tools::BuildDependencyGraph(std::span<const schema::SchemaClassRecord_t>  records,
                                                                     std::span<const schema::SchemaDependencies_t> dependencies,
                                                                     const DependencyIndexMap_t& indexMap, const DependencyKeySet_t& enums) {
	std::vector<CDependencyGraphNode> result(records.size());
	const auto                        count = std::min(records.size(), dependencies.size());
	if (records.size() != dependencies.size())
		lg::Warn("dependency", "dependency record count {} does not match class record count {}", dependencies.size(), records.size());

	for (std::size_t index = 0; index < count; ++index) {
		for (const auto& [module, names] : dependencies[index].m_Definitions) {
			for (const auto& name : names) {
				const schema::SchemaTypeName_t key{ .m_szModule = module, .m_szName = name };
				const auto                     dependencyIndex = indexMap.GetIndex(key);
				if (dependencyIndex == DependencyIndexMap_t::InvalidIndex()) {
					if (!enums.contains(key))
						lg::Warn("dependency", "invalid dep index, module {} name {}", module, name);
					continue;
				}

				++result[index];
				result[dependencyIndex].GetRequiredBy().push_back(index);
			}
		}
	}

	return result;
}

std::vector<std::size_t> tools::SortDependencyGraph(std::vector<CDependencyGraphNode>& graph) {
	std::queue<std::size_t>  ready;
	std::vector<std::size_t> result;

	result.reserve(graph.size());
	for (std::size_t index = 0; index < graph.size(); ++index)
		if (graph[index].GetDependenciesCount() == 0)
			ready.push(index);

	while (!ready.empty()) {
		const auto current = ready.front();
		ready.pop();
		result.push_back(current);

		for (const auto requiredBy : graph[current].GetRequiredBy()) {
			--graph[requiredBy];
			if (graph[requiredBy].GetDependenciesCount() == 0)
				ready.push(requiredBy);
		}
	}

	if (result.size() != graph.size())
		lg::Warn("dependency", "dependency cycle detected: sorted {} of {} types", result.size(), graph.size());

	return result;
}
