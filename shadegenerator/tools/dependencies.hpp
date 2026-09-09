#pragma once
#include <span>

#include "schema/atomicrecord.hpp"
#include "schema/classrecord.hpp"
#include "schema/dependencies.hpp"
#include "schema/model.hpp"

namespace shade::tools {
	/**
	 * @brief Builds dependencies required to emit one class definition.
	 *
	 * Base classes and by-value fields require definitions. Pointers and atomic
	 * template parameters require declarations; enum dependencies always require
	 * definitions. The resulting sets omit the class itself and declarations that
	 * are already covered by a definition dependency.
	 *
	 * @param model A `schema::CSchemaModel` that owns the referenced types.
	 * @param record A `schema::SchemaClassRecord_t` whose dependencies are resolved.
	 * @return A `schema::SchemaDependencies_t` of normalized dependencies.
	 */
	[[nodiscard]] schema::SchemaDependencies_t BuildDependencies(const schema::CSchemaModel& model, const schema::SchemaClassRecord_t& record);

	/**
	 * @brief Builds aggregate dependencies required by atomic declarations.
	 *
	 * Traverses every atomic specialization recursively and combines their
	 * dependencies because generated atomic declarations are emitted together.
	 *
	 * @param model A `schema::CSchemaModel` that owns the referenced types.
	 * @param records A `std::span<const schema::SchemaAtomicRecord_t>` to resolve.
	 * @return A `schema::SchemaDependencies_t` of normalized dependencies.
	 */
	[[nodiscard]] schema::SchemaDependencies_t BuildDependencies(const schema::CSchemaModel& model, std::span<const schema::SchemaAtomicRecord_t> records);
} // namespace shade::tools
