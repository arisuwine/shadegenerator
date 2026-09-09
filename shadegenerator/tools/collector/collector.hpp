#pragma once
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "schema/model.hpp"

class CSchemaClassInfo;
class CSchemaEnumInfo;
class CSchemaSystem;
class CSchemaSystemTypeScope;
class CSchemaType;
class CSchemaType_Atomic;

namespace shade::tools {
	class CSchemaCollector {
	private:
		struct AtomicCollectionState_t;

		schema::CSchemaModel                                            m_Model;
		std::unordered_map<const CSchemaType*, schema::SchemaTypeRef_t> m_CollectedTypes;
		std::unordered_set<std::string>                                 m_ReportedInvalidTypes;

		/**
		 * @brief Adds a builtin type to the collected schema model.
		 * @param type The builtin type kind.
		 * @return Its model type reference.
		 */
		[[nodiscard]] schema::SchemaTypeRef_t AddBuiltinType(schema::ESchemaBuiltinType type);
		/**
		 * @brief Adds an invalid type to the model and reports its name once.
		 * @param szName The unavailable type name.
		 * @return Its model type reference.
		 */
		[[nodiscard]] schema::SchemaTypeRef_t AddInvalidType(std::string szName);
		/**
		 * @brief Collects template parameters for an atomic type.
		 * @param atomic The atomic schema type.
		 * @return Its parameters, or `std::nullopt` for an invalid category.
		 */
		[[nodiscard]] std::optional<schema::SchemaAtomicParameters_t> CollectAtomicParameters(const CSchemaType_Atomic& atomic);
		/**
		 * @brief Converts a datamap field type to a schema model type.
		 * @param nType The engine `fieldtype_t` value.
		 * @return The corresponding model type reference.
		 */
		[[nodiscard]] schema::SchemaTypeRef_t CollectDataMapType(int nType);
		/**
		 * @brief Collects and caches a schema type.
		 * @param pType The source type, which may be null.
		 * @return The corresponding model type reference.
		 */
		schema::SchemaTypeRef_t CollectType(const CSchemaType* pType);
		/**
		 * @brief Collects a class record and its layout metadata.
		 * @param info The source class metadata.
		 * @return The populated class record.
		 */
		[[nodiscard]] schema::SchemaClassRecord_t CollectClass(const CSchemaClassInfo& info);
		/**
		 * @brief Collects an enum record and its enumerators.
		 * @param info The source enum metadata.
		 * @return The populated enum record.
		 */
		[[nodiscard]] schema::SchemaEnumRecord_t CollectEnum(const CSchemaEnumInfo& info);
		/**
		 * @brief Appends direct base classes and advances the layout cursor.
		 * @param info The source class metadata.
		 * @param record The record being populated.
		 * @param nCursor The current layout position.
		 */
		void CollectBaseClasses(const CSchemaClassInfo& info, schema::SchemaClassRecord_t& record, std::size_t& nCursor);
		/**
		 * @brief Appends schema fields and advances the layout cursor.
		 * @param info The source class metadata.
		 * @param record The record being populated.
		 * @param nCursor The current layout position.
		 */
		void CollectClassFields(const CSchemaClassInfo& info, schema::SchemaClassRecord_t& record, std::size_t& nCursor);
		/**
		 * @brief Resolves and appends one consecutive run of bitfields.
		 * @param info The source class metadata.
		 * @param nBegin First bitfield index.
		 * @param nEnd One past the last bitfield index.
		 * @param record The record being populated.
		 * @param cursor The current layout position.
		 */
		void CollectBitfieldRun(const CSchemaClassInfo& info, std::size_t nBegin, std::size_t nEnd, schema::SchemaClassRecord_t& record,
		                        std::size_t& cursor);
		/**
		 * @brief Appends datamap-only fields that do not duplicate schema field names.
		 * @param info The source class metadata.
		 * @param record The record being populated.
		 */
		void CollectDataMapFields(const CSchemaClassInfo& info, schema::SchemaClassRecord_t& record);
		/**
		 * @brief Adds one atomic type occurrence to its family record.
		 * @param state The aggregation state.
		 * @param szScopeName The originating type-scope name.
		 * @param pAtomic The atomic type, if present.
		 */
		void AddAtomicOccurrence(AtomicCollectionState_t& state, std::string_view szScopeName, const CSchemaType_Atomic* pAtomic);
		/**
		 * @brief Collects atomic types from every atomic map in a scope.
		 * @param state The aggregation state.
		 * @param pScope The type scope, if present.
		 */
		void CollectAtomicScope(AtomicCollectionState_t& state, const CSchemaSystemTypeScope* pScope);
		/**
		 * @brief Selects default atomic layouts and stores all accumulated records.
		 * @param state The aggregation state to finalize.
		 */
		void FinalizeAtomics(AtomicCollectionState_t& state);
		/**
		 * @brief Collects atomic type records from the global and module scopes.
		 * @param pGlobalTypeScope The global type scope.
		 * @param typeScopes The module type scopes.
		 */
		void CollectAtomics(const CSchemaSystemTypeScope* pGlobalTypeScope, std::span<CSchemaSystemTypeScope*> typeScopes);

	public:
		/**
		 * @brief Collects a complete schema model from an engine schema system.
		 *
		 * Resets prior collector state, gathers unique classes and enums, resolves
		 * their types and layouts, then collects atomic type families across scopes.
		 *
		 * @param schemaSystem The source schema system.
		 * @return The collected schema model. An empty model is returned when the
		 *         global scope or all type scopes are unavailable.
		 */
		[[nodiscard]] schema::CSchemaModel Collect(CSchemaSystem& schemaSystem);
	};
} // namespace shade::tools
