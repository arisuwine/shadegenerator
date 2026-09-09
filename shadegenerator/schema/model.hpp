#pragma once
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "atomicrecord.hpp"
#include "classrecord.hpp"
#include "enumrecord.hpp"
#include "types.hpp"

namespace shade::schema {
	/** @brief Owns the normalized schema records consumed by formatters and emitters. */
	class CSchemaModel {
	private:
		std::vector<SchemaType_t>         m_Types;
		std::vector<SchemaClassRecord_t>  m_Classes;
		std::vector<SchemaEnumRecord_t>   m_Enums;
		std::vector<SchemaAtomicRecord_t> m_Atomics;

	public:
		/** @brief Adds a type and returns its stable model reference. */
		[[nodiscard]] SchemaTypeRef_t AddType(SchemaType_t type) {
			if (m_Types.size() >= kInvalidSchemaTypeRef)
				throw std::overflow_error("schema type table exhausted");

			m_Types.emplace_back(std::move(type));
			return static_cast<SchemaTypeRef_t>(m_Types.size() - 1);
		}

		/** @brief Adds a collected class record. */
		void AddClass(SchemaClassRecord_t record) {
			m_Classes.emplace_back(std::move(record));
		}

		/** @brief Adds a collected enum record. */
		void AddEnum(SchemaEnumRecord_t record) {
			m_Enums.emplace_back(std::move(record));
		}

		/** @brief Adds a collected atomic-type record. */
		void AddAtomic(SchemaAtomicRecord_t record) {
			m_Atomics.emplace_back(std::move(record));
		}

		/** @brief Returns the type stored at a model reference. */
		[[nodiscard]] const SchemaType_t& GetType(SchemaTypeRef_t type) const {
			return m_Types.at(type);
		}

		/** @brief Returns all collected classes. */
		[[nodiscard]] std::span<const SchemaClassRecord_t> GetClasses() const noexcept {
			return m_Classes;
		}

		/** @brief Returns all collected enums. */
		[[nodiscard]] std::span<const SchemaEnumRecord_t> GetEnums() const noexcept {
			return m_Enums;
		}

		/** @brief Returns all collected atomic types. */
		[[nodiscard]] std::span<const SchemaAtomicRecord_t> GetAtomics() const noexcept {
			return m_Atomics;
		}
	};
} // namespace shade::schema
