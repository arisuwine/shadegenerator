#include "collector.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "detail.hpp"

#include "schema/types.hpp"

#include "sdk/schemasystem/schemasystem.hpp"
#include "sdk/schemasystem/schematypes.hpp"

#include "utils/debug.hpp"

namespace {
	using shade::schema::AtomicType_t;
	using shade::schema::BuiltinType_t;
	using shade::schema::DeclaredClassType_t;
	using shade::schema::DeclaredEnumType_t;
	using shade::schema::ESchemaAtomicType;
	using shade::schema::FixedArrayType_t;
	using shade::schema::InvalidType_t;
	using shade::schema::PointerType_t;
	using shade::schema::SchemaAtomicCollectionParameters_t;
	using shade::schema::SchemaAtomicIntegerParameter_t;
	using shade::schema::SchemaAtomicParameters_t;
	using shade::schema::SchemaAtomicTwoTypeParameters_t;
	using shade::schema::SchemaAtomicTypeParameter_t;
	using shade::schema::SchemaLayout_t;
	using shade::schema::SchemaTypeRef_t;
	using shade::tools::collector::ForEachMapValue;
	using shade::tools::collector::GetAtomicName;

	[[nodiscard]] ESchemaAtomicType GetAtomicType(SchemaAtomicCategory_t category) {
		switch (category) {
		case SCHEMA_ATOMIC_PLAIN:
			return ESchemaAtomicType::PLAIN;

		case SCHEMA_ATOMIC_T:
			return ESchemaAtomicType::T;

		case SCHEMA_ATOMIC_COLLECTION_OF_T:
			return ESchemaAtomicType::COLLECTION_OF_T;

		case SCHEMA_ATOMIC_TT:
			return ESchemaAtomicType::TT;

		case SCHEMA_ATOMIC_I:
			return ESchemaAtomicType::I;

		case SCHEMA_ATOMIC_INVALID:
		default:
			return ESchemaAtomicType::INVALID;
		}
	}

	struct AtomicFamilyKey_t {
		int               m_nAtomicId = -1;
		std::string       m_szName;
		ESchemaAtomicType m_eType = ESchemaAtomicType::INVALID;

		auto operator<=>(const AtomicFamilyKey_t&) const = default;
	};

	[[nodiscard]] bool EquivalentTypes(const shade::schema::CSchemaModel& model, SchemaTypeRef_t lhs, SchemaTypeRef_t rhs,
	                                   std::unordered_set<std::uint64_t>& visited);

	[[nodiscard]] bool EquivalentParameters(const shade::schema::CSchemaModel& model, const SchemaAtomicParameters_t& lhs,
	                                        const SchemaAtomicParameters_t& rhs, std::unordered_set<std::uint64_t>& visited) {
		if (lhs.index() != rhs.index())
			return false;

		if (std::holds_alternative<std::monostate>(lhs))
			return true;

		if (const auto* left = std::get_if<SchemaAtomicTypeParameter_t>(&lhs))
			return EquivalentTypes(model, left->m_Type, std::get<SchemaAtomicTypeParameter_t>(rhs).m_Type, visited);

		if (const auto* left = std::get_if<SchemaAtomicCollectionParameters_t>(&lhs)) {
			const auto& right = std::get<SchemaAtomicCollectionParameters_t>(rhs);
			return left->m_nElementSize == right.m_nElementSize && left->m_nFixedBufferCount == right.m_nFixedBufferCount &&
			       EquivalentTypes(model, left->m_ElementType, right.m_ElementType, visited);
		}
		if (const auto* left = std::get_if<SchemaAtomicTwoTypeParameters_t>(&lhs)) {
			const auto& right = std::get<SchemaAtomicTwoTypeParameters_t>(rhs);
			return EquivalentTypes(model, left->m_FirstType, right.m_FirstType, visited) &&
			       EquivalentTypes(model, left->m_SecondType, right.m_SecondType, visited);
		}
		return std::get<SchemaAtomicIntegerParameter_t>(lhs).m_nValue == std::get<SchemaAtomicIntegerParameter_t>(rhs).m_nValue;
	}

	[[nodiscard]] bool EquivalentTypes(const shade::schema::CSchemaModel& model, SchemaTypeRef_t lhs, SchemaTypeRef_t rhs,
	                                   std::unordered_set<std::uint64_t>& visited) {
		if (lhs == rhs)
			return true;

		const std::uint64_t pair = (static_cast<std::uint64_t>(lhs) << 32U) | rhs;
		if (!visited.emplace(pair).second)
			return true;

		const auto& left  = model.GetType(lhs);
		const auto& right = model.GetType(rhs);
		if (left.index() != right.index())
			return false;

		if (const auto value = std::get_if<BuiltinType_t>(&left))
			return value->m_eType == std::get<BuiltinType_t>(right).m_eType;

		if (const auto value = std::get_if<DeclaredClassType_t>(&left))
			return value->m_Name == std::get<DeclaredClassType_t>(right).m_Name;

		if (const auto value = std::get_if<DeclaredEnumType_t>(&left))
			return value->m_Name == std::get<DeclaredEnumType_t>(right).m_Name;

		if (const auto value = std::get_if<PointerType_t>(&left))
			return EquivalentTypes(model, value->m_PointeeType, std::get<PointerType_t>(right).m_PointeeType, visited);

		if (const auto value = std::get_if<FixedArrayType_t>(&left)) {
			const auto& other = std::get<FixedArrayType_t>(right);
			return value->m_nCount == other.m_nCount && EquivalentTypes(model, value->m_ElementType, other.m_ElementType, visited);
		}

		if (const auto value = std::get_if<AtomicType_t>(&left)) {
			const auto& other = std::get<AtomicType_t>(right);
			return value->m_szName == other.m_szName && EquivalentParameters(model, value->m_Parameters, other.m_Parameters, visited);
		}

		return std::get<InvalidType_t>(left).m_szName == std::get<InvalidType_t>(right).m_szName;
	}

	[[nodiscard]] bool EquivalentParameters(const shade::schema::CSchemaModel& model, const SchemaAtomicParameters_t& lhs,
	                                        const SchemaAtomicParameters_t& rhs) {
		std::unordered_set<std::uint64_t> visited;
		return EquivalentParameters(model, lhs, rhs, visited);
	}

} // namespace

struct shade::tools::CSchemaCollector::AtomicCollectionState_t {
	std::map<AtomicFamilyKey_t, schema::SchemaAtomicRecord_t> m_Records;
	std::unordered_set<std::string>                           m_ReportedStandardLibraryAtomics;
};

void shade::tools::CSchemaCollector::CollectAtomics(const CSchemaSystemTypeScope* pGlobalTypeScope, std::span<CSchemaSystemTypeScope*> typeScopes) {
	AtomicCollectionState_t state;
	CollectAtomicScope(state, pGlobalTypeScope);
	for (const auto* scope : typeScopes)
		CollectAtomicScope(state, scope);
	FinalizeAtomics(state);
}

void shade::tools::CSchemaCollector::AddAtomicOccurrence(AtomicCollectionState_t& state, std::string_view szScopeName, const CSchemaType_Atomic* pAtomic) {
	if (!pAtomic)
		return;

	const std::string name = GetAtomicName(pAtomic);
	if (std::string_view(name).starts_with("std::")) {
		if (state.m_ReportedStandardLibraryAtomics.emplace(name).second)
			lg::Warn("atomics", "standard-library atomic '{}' encountered in type scope '{}'; skipping it", name, szScopeName);
		return;
	}

	const auto type = GetAtomicType(pAtomic->m_eAtomicCategory);
	if (type == schema::ESchemaAtomicType::INVALID)
		return;

	const AtomicFamilyKey_t key{
		.m_nAtomicId = pAtomic->m_nAtomicID,
		.m_szName    = name,
		.m_eType     = type,
	};

	auto [position, inserted] = state.m_Records.try_emplace(key);
	if (inserted) {
		position->second.m_szName = name;
		position->second.m_nId    = pAtomic->m_nAtomicID;
		position->second.m_eType  = type;
	}

	const SchemaLayout_t layout{
		.m_nSize      = pAtomic->m_nSize,
		.m_nAlignment = pAtomic->m_nAlignment,
	};

	if (type == schema::ESchemaAtomicType::PLAIN) {
		if (!position->second.m_DefaultLayout)
			position->second.m_DefaultLayout = layout;
		else if (*position->second.m_DefaultLayout != layout)
			lg::Warn("atomics", "conflicting layout for plain atomic '{}' from scope '{}'; keeping the first layout", name, szScopeName);
		return;
	}

	auto parameters = CollectAtomicParameters(*pAtomic);
	if (!parameters)
		return;

	auto&      specializations = position->second.m_Specializations;
	const auto existing        = std::find_if(specializations.begin(), specializations.end(), [&](const auto& specialization) {
		return EquivalentParameters(m_Model, specialization.m_Parameters, *parameters);
	});
	if (existing != specializations.end()) {
		if (!existing->m_LayoutOverride || *existing->m_LayoutOverride != layout)
			lg::Warn("atomics", "conflicting layout for '{}' specialization from scope '{}'; keeping the first layout", name, szScopeName);
		return;
	}

	specializations.push_back({
	    .m_Parameters     = *parameters,
	    .m_LayoutOverride = layout,
	});
}

void shade::tools::CSchemaCollector::CollectAtomicScope(AtomicCollectionState_t& state, const CSchemaSystemTypeScope* pScope) {
	if (!pScope)
		return;

	const std::string_view scopeName = pScope->GetName().empty() ? "unnamed" : pScope->GetName();
	ForEachMapValue(pScope->m_Atomic, [&](const auto* atomic) { AddAtomicOccurrence(state, scopeName, atomic); });
	ForEachMapValue(pScope->m_Atomic_T, [&](const auto* atomic) { AddAtomicOccurrence(state, scopeName, atomic); });
	ForEachMapValue(pScope->m_Atomic_CollectionOfT, [&](const auto* atomic) { AddAtomicOccurrence(state, scopeName, atomic); });
	ForEachMapValue(pScope->m_Atomic_TT, [&](const auto* atomic) { AddAtomicOccurrence(state, scopeName, atomic); });
	ForEachMapValue(pScope->m_Atomic_I, [&](const auto* atomic) { AddAtomicOccurrence(state, scopeName, atomic); });
}

void shade::tools::CSchemaCollector::FinalizeAtomics(AtomicCollectionState_t& state) {
	for (auto& [_, record] : state.m_Records) {
		if (!record.m_Specializations.empty()) {
			std::vector<std::pair<SchemaLayout_t, std::size_t>> frequencies;
			for (const auto& specialization : record.m_Specializations) {
				if (!specialization.m_LayoutOverride)
					continue;

				const auto it = std::find_if(frequencies.begin(), frequencies.end(),
				                             [&](const auto& entry) { return entry.first == *specialization.m_LayoutOverride; });
				if (it == frequencies.end())
					frequencies.emplace_back(*specialization.m_LayoutOverride, 1);
				else
					++it->second;
			}

			const auto mostCommon =
			    std::max_element(frequencies.begin(), frequencies.end(), [](const auto& lhs, const auto& rhs) { return lhs.second < rhs.second; });
			if (mostCommon != frequencies.end()) {
				record.m_DefaultLayout = mostCommon->first;
				for (auto& specialization : record.m_Specializations) {
					if (specialization.m_LayoutOverride == record.m_DefaultLayout)
						specialization.m_LayoutOverride.reset();
				}
			}
		}

		m_Model.AddAtomic(std::move(record));
	}
}
