#pragma once
#include <cstddef>
#include <optional>
#include <string>

#include "schema/types.hpp"

#include "sdk/schemasystem/schematypes.hpp"

class CSchemaEnumInfo;
class CSchemaType_Atomic;

namespace shade::tools::collector {

	/**
	 * @brief Builds a descriptive name for an enum with an unsupported underlying type.
	 *
	 * Includes the enum's module, name, and underlying type width so that the
	 * collector can preserve the type as invalid while reporting a useful reason.
	 *
	 * @param info Schema metadata for the enum.
	 * @return A human-readable invalid-type name.
	 */
	[[nodiscard]] std::string GetUnsupportedEnumTypeName(const CSchemaEnumInfo& info);
	/**
	 * @brief Converts a signed size to a non-negative size value.
	 *
	 * Negative and zero values are treated as zero to prevent invalid schema
	 * sizes and offsets from becoming large unsigned values.
	 *
	 * @param nValue The signed size or offset.
	 * @return `nValue` when it is positive; otherwise zero.
	 */
	[[nodiscard]] std::size_t NonNegativeSize(int nValue);
	/**
	 * @brief Finds the builtin integer type with a given width and signedness.
	 *
	 * Supports 8-, 16-, 32-, and 64-bit integer types only.
	 *
	 * @param nSize The integer size in bytes.
	 * @param bIsSigned Whether the requested integer type is signed.
	 * @return The matching builtin type, or `std::nullopt` for an unsupported width.
	 */
	[[nodiscard]] std::optional<schema::ESchemaBuiltinType> GetIntegerType(std::size_t nSize, bool bIsSigned);
	/**
	 * @brief Normalizes an atomic schema type name for generated output.
	 *
	 * Removes template arguments and trailing whitespace. Standard-library names
	 * are retained, while namespace separators in other names become `__`.
	 *
	 * @param pAtomic The atomic schema type to name.
	 * @return The normalized atomic type name.
	 */
	[[nodiscard]] std::string GetAtomicName(const CSchemaType_Atomic* pAtomic);

	/**
	 * @brief Invokes a function for every value in a schema pointer map.
	 *
	 * Traverses the map in its in-order iteration order and forwards each stored
	 * value to the supplied callable.
	 *
	 * @tparam K The map key type.
	 * @tparam V The map value type.
	 * @tparam fn The callable type.
	 * @param values The schema pointer map to traverse.
	 * @param function The function invoked for each map value.
	 */
	template <typename K, typename V, typename fn>
	void ForEachMapValue(const CSchemaPtrMap<K, V>& values, fn&& function) {
		const auto& map = values.m_Map;
		for (auto index = map.FirstInorder(); index != map.InvalidIndex(); index = map.NextInorder(index))
			function(map.Element(index));
	}

} // namespace shade::tools::collector
