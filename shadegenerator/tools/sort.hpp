#pragma once
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "schema/classrecord.hpp"
#include "schema/dependencies.hpp"

namespace shade::tools {
	class CDependencyGraphNode {
	private:
		std::size_t              m_nDependenciesCount = 0;
		std::vector<std::size_t> m_RequiredBy;

	public:
		/**
		 * @brief Increments the number of unresolved dependencies.
		 * @return A `CDependencyGraphNode&` referring to this node.
		 */
		CDependencyGraphNode& operator++() {
			++m_nDependenciesCount;
			return *this;
		}

		/**
		 * @brief Decrements the number of unresolved dependencies.
		 * @return A `CDependencyGraphNode&` referring to this node.
		 */
		CDependencyGraphNode& operator--() {
			--m_nDependenciesCount;
			return *this;
		}

		/**
		 * @brief Returns the indices of nodes that depend on this node.
		 * @return A `std::vector<std::size_t>&` of dependent node indices.
		 */
		[[nodiscard]] std::vector<std::size_t>& GetRequiredBy() noexcept {
			return m_RequiredBy;
		}

		/**
		 * @brief Returns the indices of nodes that depend on this node.
		 * @return A `const std::vector<std::size_t>&` of dependent node indices.
		 */
		[[nodiscard]] const std::vector<std::size_t>& GetRequiredBy() const noexcept {
			return m_RequiredBy;
		}

		/**
		 * @brief Returns the number of dependencies not yet resolved.
		 * @return A `std::size_t` unresolved dependency count.
		 */
		[[nodiscard]] std::size_t GetDependenciesCount() const noexcept {
			return m_nDependenciesCount;
		}
	};

	struct SchemaTypeNameHash_t {
		/**
		 * @brief Computes a hash for a module-qualified schema type name.
		 * @param name A `schema::SchemaTypeName_t` to hash.
		 * @return A `std::size_t` combining the module and type-name hashes.
		 */
		[[nodiscard]] std::size_t operator()(const schema::SchemaTypeName_t& name) const noexcept {
			const auto moduleHash = std::hash<std::string>{}(name.m_szModule);
			const auto nameHash   = std::hash<std::string>{}(name.m_szName);
			return moduleHash ^ (nameHash + 0x9e3779b9 + (moduleHash << 6) + (moduleHash >> 2));
		}
	};

	struct DependencyIndexMap_t {
		static constexpr std::size_t kInvalidIndex = static_cast<std::size_t>(-1);

		std::unordered_map<schema::SchemaTypeName_t, std::size_t, SchemaTypeNameHash_t> m_Map;

		/**
		 * @brief Looks up a class-record index by its qualified name.
		 * @param key A `schema::SchemaTypeName_t` containing a qualified class name.
		 * @return A `std::size_t` matching index, or `kInvalidIndex` if absent.
		 */
		[[nodiscard]] std::size_t GetIndex(const schema::SchemaTypeName_t& key) const {
			if (const auto it = m_Map.find(key); it != m_Map.end())
				return it->second;

			return kInvalidIndex;
		}

		/**
		 * @brief Returns the sentinel used for a missing index.
		 * @return A `std::size_t` invalid index value.
		 */
		[[nodiscard]] static constexpr std::size_t InvalidIndex() noexcept {
			return kInvalidIndex;
		}

		/**
		 * @brief Adds a class name-to-index mapping.
		 * @param key A `schema::SchemaTypeName_t` containing a qualified class name.
		 * @param index A `std::size_t` class-record index.
		 * @return A `bool`: `true` if the key was inserted; otherwise `false`.
		 */
		[[nodiscard]] bool Add(schema::SchemaTypeName_t key, std::size_t index) {
			return m_Map.emplace(std::move(key), index).second;
		}
	};

	using DependencyKeySet_t = std::unordered_set<schema::SchemaTypeName_t, SchemaTypeNameHash_t>;

	struct SortedDependencyGroup_t {
		std::string_view              m_szModule;
		std::vector<std::string_view> m_Names;
	};

	/**
	 * @brief Creates a lexically sorted view of dependency groups.
	 *
	 * Sorts modules and the names within each module without mutating the source.
	 * Returned views remain valid while the input dependency map is unchanged.
	 *
	 * @param dependencies A `schema::SchemaDependencyMap_t` to order.
	 * @return A `std::vector<SortedDependencyGroup_t>` containing sorted groups.
	 */
	[[nodiscard]] std::vector<SortedDependencyGroup_t> GetSortedDependencyGroups(const schema::SchemaDependencyMap_t& dependencies);

	/**
	 * @brief Builds a lookup table from class names to record indices.
	 *
	 * Keeps the first index for duplicate names and reports subsequent duplicates.
	 *
	 * @param records A `std::span<const schema::SchemaClassRecord_t>` to index.
	 * @return A populated `DependencyIndexMap_t`.
	 */
	[[nodiscard]] DependencyIndexMap_t BuildDependencyIndexMap(std::span<const schema::SchemaClassRecord_t> records);

	/**
	 * @brief Builds a directed graph from class definition dependencies.
	 *
	 * Each node counts the classes it requires, and stores the classes that
	 * require it. Enum definitions are excluded because they are not graph nodes.
	 *
	 * @param records A `std::span<const schema::SchemaClassRecord_t>` for graph nodes.
	 * @param dependencies A `std::span<const schema::SchemaDependencies_t>`.
	 * @param indexMap A `DependencyIndexMap_t` for class dependency names.
	 * @param enums A `DependencyKeySet_t` of qualified enum names.
	 * @return A `std::vector<CDependencyGraphNode>` in class-record order.
	 */
	[[nodiscard]] std::vector<CDependencyGraphNode> BuildDependencyGraph(std::span<const schema::SchemaClassRecord_t>  records,
	                                                                     std::span<const schema::SchemaDependencies_t> dependencies,
	                                                                     const DependencyIndexMap_t& indexMap, const DependencyKeySet_t& enums);

	/**
	 * @brief Topologically sorts a dependency graph.
	 *
	 * Mutates dependency counts while processing ready nodes. Cyclic nodes are
	 * omitted from the result and cause a warning to be reported.
	 *
	 * @param graph A `std::vector<CDependencyGraphNode>` to sort in place.
	 * @return A `std::vector<std::size_t>` of indices in dependency-safe order.
	 */
	[[nodiscard]] std::vector<std::size_t> SortDependencyGraph(std::vector<CDependencyGraphNode>& graph);
} // namespace shade::tools
