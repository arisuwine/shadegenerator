#pragma once
#include <optional>
#include <span>
#include <vector>

#include "sdk/schemasystem/schematypes.hpp"

namespace shade::tools {
	/**
	 * @brief Describes the placement of a single bitfield.
	 */
	struct BitfieldPlacement_t {
		size_t m_nStorageBits = 0;
		size_t m_nByteOffset  = 0;
	};

	/**
	 * @brief Describes a resolved bitfield layout.
	 */
	struct BitfieldLayout_t {
		std::vector<BitfieldPlacement_t> m_Placements;
		size_t                           m_nSize = 0;
	};

	/**
	 * @brief Represents an intermediate bitfield layout during resolution.
	 *
	 * This structure stores the current candidate layout and the metrics used
	 * to compare it with other candidates.
	 */
	struct BitfieldLayoutState_t {
		std::vector<BitfieldPlacement_t> m_Placements;
		size_t                           m_nSize           = 0;
		size_t                           m_nTypeSwitches   = 0;
		size_t                           m_nTypeWidthScore = 0;
		size_t                           m_nStorageBits    = 0;
		size_t                           m_nUsedBits       = 0;
	};

	/**
	 * @brief Resolves the memory layout of a bitfield.
	 *
	 * Reconstructs the most compact sequence of MSVC-style allocation units
	 * that fits before the next known schema offset. The generated class must
	 * use `#pragma pack(1)` because this resolver places all byte-alignment
	 * padding explicitly.
	 *
	 * Fields never straddle an allocation unit. Among layouts of equal size,
	 * the resolver prefers fewer type switches and then narrower integral types.
	 *
	 * @param fields A `std::span` of `SchemaClassFieldData_t` objects.
	 * @param nAvailableBytes The number of bytes available for the bitfield.
	 * @return A `tools::BitfieldLayout_t` describing the resolved layout, or
	 *        `std::nullopt` if the fields cannot fit within the available space.
	 *
	 * @see SchemaClassFieldData_t
	 */
	[[nodiscard]] std::optional<BitfieldLayout_t> ResolveBitfieldLayout(std::span<const SchemaClassFieldData_t> fields, size_t nAvailableBytes);

	/**
	 * @brief Determines whether a field represents a bitfield based on its category.
	 *
	 * @param field A reference to a `SchemaClassFieldData_t` object.
	 * @return `true` if the field is a bitfield; otherwise, `false`.
	 */
	[[nodiscard]] bool IsBitfield(const SchemaClassFieldData_t& field);
} // namespace shade::tools
