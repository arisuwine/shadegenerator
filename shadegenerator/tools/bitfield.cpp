#include "bitfield.hpp"

#include <algorithm>
#include <array>
#include <string_view>

using namespace shade;

namespace {
	bool IsBetterBitfieldLayout(const tools::BitfieldLayoutState_t& lhs, const tools::BitfieldLayoutState_t& rhs) {
		return std::tie(lhs.m_nSize, lhs.m_nTypeSwitches, lhs.m_nTypeWidthScore) < std::tie(rhs.m_nSize, rhs.m_nTypeSwitches, rhs.m_nTypeWidthScore);
	}

	constexpr auto kBitfieldTypes = std::to_array<std::pair<size_t, std::string_view>>(
	    { { 8, "std::uint8_t" }, { 16, "std::uint16_t" }, { 32, "std::uint32_t" }, { 64, "std::uint64_t" } });
} // namespace

bool tools::IsBitfield(const SchemaClassFieldData_t& field) {
	return field.m_pType && field.m_pType->m_eTypeCategory == SCHEMA_TYPE_BITFIELD;
}

std::optional<tools::BitfieldLayout_t> tools::ResolveBitfieldLayout(std::span<const SchemaClassFieldData_t> fields, size_t nAvailableBytes) {
	std::vector<BitfieldLayoutState_t> states(1);

	for (const auto& field : fields) {
		if (!IsBitfield(field))
			return std::nullopt;

		const int nRawWidth = field.m_pType->Cast<CSchemaType_Bitfield>()->m_nBitfieldCount;
		if (nRawWidth <= 0)
			return std::nullopt;

		const size_t                       nFieldBits = static_cast<size_t>(nRawWidth);
		std::vector<BitfieldLayoutState_t> nextStates;

		for (const auto& state : states) {
			for (const auto& [nStorageBits, _] : kBitfieldTypes) {
				if (nFieldBits > nStorageBits)
					continue;

				BitfieldLayoutState_t candidate   = state;
				size_t                nUnitOffset = 0;

				if (state.m_nStorageBits == nStorageBits && state.m_nUsedBits + nFieldBits <= nStorageBits) {
					nUnitOffset = state.m_nSize - nStorageBits / 8;
					candidate.m_nUsedBits += nFieldBits;
				} else {
					nUnitOffset = state.m_nSize;
					candidate.m_nSize += nStorageBits / 8;
					candidate.m_nUsedBits = nFieldBits;
					if (state.m_nStorageBits != 0 && state.m_nStorageBits != nStorageBits)
						candidate.m_nTypeSwitches++;
				}

				if (candidate.m_nSize > nAvailableBytes)
					continue;

				candidate.m_nStorageBits = nStorageBits;
				candidate.m_nTypeWidthScore += nStorageBits;
				candidate.m_Placements.push_back({ nStorageBits, nUnitOffset });

				auto sameState = std::find_if(nextStates.begin(), nextStates.end(), [&](const BitfieldLayoutState_t& other) {
					return other.m_nStorageBits == candidate.m_nStorageBits && other.m_nUsedBits == candidate.m_nUsedBits;
				});
				if (sameState == nextStates.end())
					nextStates.push_back(std::move(candidate));
				else if (IsBetterBitfieldLayout(candidate, *sameState))
					*sameState = std::move(candidate);
			}
		}

		if (nextStates.empty())
			return std::nullopt;

		states = std::move(nextStates);
	}

	const auto best = std::min_element(states.begin(), states.end(), IsBetterBitfieldLayout);
	if (best == states.end())
		return std::nullopt;

	return BitfieldLayout_t{ best->m_Placements, best->m_nSize };
}