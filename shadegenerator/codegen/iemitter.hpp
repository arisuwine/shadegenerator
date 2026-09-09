#pragma once

#include "schema/atomicrecord.hpp"
#include "schema/classrecord.hpp"
#include "schema/enumrecord.hpp"

namespace shade::codegen {
	/**
	 * @brief Minimal format-independent schema emitter interface.
	 */
	class IEmitter {
	public:
		/** @brief Writes declarations required before individual schema records. */
		virtual void Prologue() = 0;
		/** @brief Emits one class record. */
		virtual void Class(const schema::SchemaClassRecord_t& record) = 0;
		/** @brief Emits one enum record. */
		virtual void Enum(const schema::SchemaEnumRecord_t& record) = 0;
		/** @brief Emits one atomic-type record. */
		virtual void Atomic(const schema::SchemaAtomicRecord_t& record) = 0;

		/** @brief Returns the extension used for files produced by this emitter. */
		[[nodiscard]] virtual std::string_view GetExtension() const noexcept = 0;

		/** @brief Enables safe destruction through the emitter interface. */
		virtual ~IEmitter() = default;
	};
} // namespace shade::codegen
