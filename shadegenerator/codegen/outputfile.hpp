#pragma once
#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace fs = std::filesystem;

namespace shade {
	namespace codegen {
		/**
		 * @brief Provides output-file handling for generated code.
		 *
		 * Creates missing parent directories, opens the specified file, and exposes
		 * stream-like output operations.
		 */
		class COutputFile {
		private:
			std::ofstream m_Stream;

		public:
			/**
			 * @brief Opens an output file and creates any missing parent directories.
			 * @param path Destination file path.
			 * @throws std::runtime_error if the file cannot be opened.
			 */
			explicit COutputFile(const fs::path& path) {
				if (const auto parent = path.parent_path(); !parent.empty())
					fs::create_directories(parent);

				m_Stream.open(path);

				if (!m_Stream.is_open())
					throw std::runtime_error(std::format("failed to create output file: {}", path.string()));
			}

			/** @brief Forwards a value to the underlying output stream. */
			template <typename T>
			COutputFile& operator<<(T&& value) {
				m_Stream << std::forward<T>(value);
				return *this;
			}

			/** @brief Returns the underlying output stream. */
			std::ofstream& GetStream() noexcept {
				return m_Stream;
			}
		};
	} // namespace codegen
} // namespace shade
