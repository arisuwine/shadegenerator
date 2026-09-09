#include "dependencies.hpp"

#include <concepts>
#include <type_traits>
#include <variant>

namespace {
	enum class EDependencyRequirement : std::uint8_t {
		Definition,
		Declaration
	};

	using shade::schema::AtomicType_t;
	using shade::schema::BuiltinType_t;
	using shade::schema::DeclaredClassType_t;
	using shade::schema::DeclaredEnumType_t;
	using shade::schema::FixedArrayType_t;
	using shade::schema::InvalidType_t;
	using shade::schema::PointerType_t;
	using shade::schema::SchemaAtomicCollectionParameters_t;
	using shade::schema::SchemaAtomicParameters_t;
	using shade::schema::SchemaAtomicTwoTypeParameters_t;
	using shade::schema::SchemaAtomicTypeParameter_t;
	using shade::schema::SchemaDependencies_t;
	using shade::schema::SchemaTypeName_t;
	using shade::schema::SchemaTypeRef_t;

	void AddDependency(SchemaDependencies_t& dependencies, const SchemaTypeName_t& name, EDependencyRequirement requirement) {
		auto& bucket = requirement == EDependencyRequirement::Definition ? dependencies.m_Definitions : dependencies.m_Declarations;
		bucket[name.m_szModule].insert(name.m_szName);
	}

	void ResolveTypeDependencies(const shade::schema::CSchemaModel& model, SchemaTypeRef_t type, EDependencyRequirement requirement,
	                             SchemaDependencies_t& dependencies);

	void ResolveAtomicParameters(const shade::schema::CSchemaModel& model, const SchemaAtomicParameters_t& parameters,
	                             SchemaDependencies_t& dependencies) {
		std::visit(
		    [&]<typename T>(const T& value) {
			    using Value_t = std::remove_cvref_t<T>;
			    if constexpr (std::same_as<Value_t, SchemaAtomicTypeParameter_t>) {
				    ResolveTypeDependencies(model, value.m_Type, EDependencyRequirement::Declaration, dependencies);
			    } else if constexpr (std::same_as<Value_t, SchemaAtomicCollectionParameters_t>) {
				    ResolveTypeDependencies(model, value.m_ElementType, EDependencyRequirement::Declaration, dependencies);
			    } else if constexpr (std::same_as<Value_t, SchemaAtomicTwoTypeParameters_t>) {
				    ResolveTypeDependencies(model, value.m_FirstType, EDependencyRequirement::Declaration, dependencies);
				    ResolveTypeDependencies(model, value.m_SecondType, EDependencyRequirement::Declaration, dependencies);
			    }
		    },
		    parameters);
	}

	void ResolveTypeDependencies(const shade::schema::CSchemaModel& model, SchemaTypeRef_t type, EDependencyRequirement requirement,
	                             SchemaDependencies_t& dependencies) {
		if (type == shade::schema::kInvalidSchemaTypeRef)
			return;

		std::visit(
		    [&]<typename T>(const T& value) {
			    using Value_t = std::remove_cvref_t<T>;
			    if constexpr (std::same_as<Value_t, BuiltinType_t> || std::same_as<Value_t, InvalidType_t>) {
				    return;
			    } else if constexpr (std::same_as<Value_t, DeclaredClassType_t>) {
				    AddDependency(dependencies, value.m_Name, requirement);
			    } else if constexpr (std::same_as<Value_t, DeclaredEnumType_t>) {
				    AddDependency(dependencies, value.m_Name, EDependencyRequirement::Definition);
			    } else if constexpr (std::same_as<Value_t, PointerType_t>) {
				    ResolveTypeDependencies(model, value.m_PointeeType, EDependencyRequirement::Declaration, dependencies);
			    } else if constexpr (std::same_as<Value_t, FixedArrayType_t>) {
				    ResolveTypeDependencies(model, value.m_ElementType, requirement, dependencies);
			    } else if constexpr (std::same_as<Value_t, AtomicType_t>) {
				    ResolveAtomicParameters(model, value.m_Parameters, dependencies);
			    }
		    },
		    model.GetType(type));
	}

	void NormalizeDependencies(SchemaDependencies_t& dependencies, const SchemaTypeName_t* self = nullptr) {
		if (self) {
			for (auto* map : { &dependencies.m_Definitions, &dependencies.m_Declarations }) {
				if (const auto it = map->find(self->m_szModule); it != map->end())
					it->second.erase(self->m_szName);
			}
		}

		for (const auto& [module, names] : dependencies.m_Definitions) {
			if (const auto declarations = dependencies.m_Declarations.find(module); declarations != dependencies.m_Declarations.end())
				for (const auto& name : names)
					declarations->second.erase(name);
		}

		std::erase_if(dependencies.m_Definitions, [](const auto& entry) { return entry.second.empty(); });
		std::erase_if(dependencies.m_Declarations, [](const auto& entry) { return entry.second.empty(); });
	}
} // namespace

shade::schema::SchemaDependencies_t shade::tools::BuildDependencies(const schema::CSchemaModel& model, const schema::SchemaClassRecord_t& record) {
	schema::SchemaDependencies_t result;

	for (const auto& base : record.m_BaseClasses)
		AddDependency(result, base.m_Name, EDependencyRequirement::Definition);

	for (const auto& field : record.m_Fields)
		ResolveTypeDependencies(model, field.m_Type, EDependencyRequirement::Definition, result);

	for (const auto& field : record.m_DataMapFields)
		ResolveTypeDependencies(model, field.m_Type, EDependencyRequirement::Definition, result);

	NormalizeDependencies(result, &record.m_Name);

	return result;
}

shade::schema::SchemaDependencies_t shade::tools::BuildDependencies(const schema::CSchemaModel&                   model,
                                                                    std::span<const schema::SchemaAtomicRecord_t> records) {
	schema::SchemaDependencies_t result;

	for (const auto& record : records)
		for (const auto& specialization : record.m_Specializations)
			ResolveAtomicParameters(model, specialization.m_Parameters, result);

	NormalizeDependencies(result);

	return result;
}
