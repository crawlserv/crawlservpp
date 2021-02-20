/*
 *
 * ---
 *
 *  Copyright (C) 2021 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * TextMap.hpp
 *
 * Text map (entries) used to annotate parts of a corpus (e.g. to store the dates or articles contained in these parts).
 *
 *  Created on: Mar 4, 2020
 *      Author: ans
 */

#ifndef STRUCT_TEXTMAP_HPP_
#define STRUCT_TEXTMAP_HPP_

#include "../Helper/Memory.hpp"

#include <cstddef>	// std::size_t
#include <string>	// std::string
#include <utility>	// std::pair, std::swap
#include <vector>	// std::vector

namespace crawlservpp::Struct {

	//! Text map entry.
	/*!
	 * A text map entry annotates one part
	 *  of a text, defined by its position
	 *   and length, with a string value.
	 */
	struct TextMapEntry {
		///@name Properties
		///@{

		//! The position of the annotated part inside the text.
		/*!
		 * Zero indicates the very beginning
		 *  of the text.
		 */
		std::size_t p{};

		//! The length of the annotated part inside the text.
		std::size_t l{};

		//! Value of the annotation.
		/*!
		 * For example, an article ID or a date
		 *  for an entry of the article or date
		 *  map belonging to a text corpus.
		 */
		std::string value;

		///@}
		///@name Construction
		///@{

		//! Default constructor.
		TextMapEntry() = default;

		//! Constructor creating an empty annotation.
		/*!
		 * \param setPos The position of the
		 *   annotated part of the text,
		 *   starting with zero at the
		 *   beginning of the text.
		 * \param setLength The length of the
		 *   annotated part of the text.
		 */
		TextMapEntry(
				std::size_t setPos,
				std::size_t setLength
		) : p(setPos), l(setLength) {}

		//! Constructor creating a non-empty annotation.
		/*!
		 * \param setPos The position of the
		 *   annotated part of the text,
		 *   starting with zero at the
		 *   beginning of the text.
		 * \param setLength The length of the
		 *   annotated part of the text.
		 * \param setValue Const reference
		 *   to a string containing the
		 *   value of the annotation.
		 */
		TextMapEntry(
				std::size_t setPos,
				std::size_t setLength,
				const std::string& setValue
		) : p(setPos), l(setLength), value(setValue) {}

		///@}
		///@name Swap
		///@{

		//! Swaps the text map entry with another.
		/*!
		 * \param other Reference to the other
		 *   text map entry to be swapped with.
		 */
		void swap(TextMapEntry& other) {
			std::swap(*this, other);
		}

		///@}
		///@name Cleanup
		///@{

		//! Resets its properties to their default values and frees the memory used by the entry.
		void free() {
			this->p = 0;
			this->l = 0;

			Helper::Memory::free(this->value);
		}

		///@}
		///@name Static Access Functions
		///@{

		//! Gets a reference to the position of a text map entry.
		/*!
		 * \param entry Reference to the text map
		 *   entry to get the position of.
		 *
		 * \returns Reference to the position of
		 *   the given entry.
		 *
		 */
		static std::size_t& pos(TextMapEntry& entry) {
			return entry.p;
		}

		//! Gets a reference to the position of a sentence map entry.
		/*!
		 * \param entry Reference to the sentence
		 *   map entry to get the position of.
		 *
		 * \returns Reference to the position of
		 *   the given entry.
		 *
		 */
		static std::size_t& pos(std::pair<std::size_t, std::size_t>& entry) {
			return entry.first;
		}

		//! Gets the position of a text map entry.
		/*!
		 * \param entry Constant reference to the
		 *   text map entry to get the position of.
		 *
		 * \returns The position of the given entry.
		 *
		 */
		static std::size_t pos(const TextMapEntry& entry) {
			return entry.p;
		}

		//! Gets the position of a sentence map entry.
		/*!
		 * \param entry Constant reference to the
		 *   sentence map entry to get the position
		 *   of.
		 *
		 * \returns The position of the given entry.
		 *
		 */
		static std::size_t pos(const std::pair<std::size_t, std::size_t>& entry) {
			return entry.first;
		}

		//! Gets the end of a map entry.
		/*!
		 * \param entry Constant reference to the
		 *   map entry to get the end of.
		 *
		 * \returns The end of the given entry.
		 */
		template<typename T> static std::size_t end(const T& entry) {
			return TextMapEntry::pos(entry) + TextMapEntry::length(entry);
		}

		//! Gets a reference to the length of a text map entry.
		/*!
		 * \param entry Reference to the text map
		 *   entry to get the length of.
		 *
		 * \returns Reference to the length of the
		 *   given entry.
		 *
		 */
		static std::size_t& length(TextMapEntry& entry) {
			return entry.l;
		}

		//! Gets a reference to the length of a sentence map entry.
		/*!
		 * \param entry Reference to the sentence
		 *   map entry to get the length of.
		 *
		 * \returns Reference to the length of the
		 *   given entry.
		 *
		 */
		static std::size_t& length(std::pair<std::size_t, std::size_t>& entry) {
			return entry.second;
		}

		//! Gets the length of a text map entry.
		/*!
		 * \param entry Constant reference to the
		 *   text map entry to get the length of.
		 *
		 * \returns The length of the given entry.
		 *
		 */
		static std::size_t length(const TextMapEntry& entry) {
			return entry.l;
		}

		//! Gets the length of a sentence map entry.
		/*!
		 * \param entry Constant reference to the
		 *   sentence map entry to get the length
		 *   of.
		 *
		 * \returns The length of the given entry.
		 *
		 */
		static std::size_t length(const std::pair<std::size_t, std::size_t>& entry) {
			return entry.second;
		}

		///@}
	};

	//! A text map is defined as a vector of text map entries.
	using TextMap = std::vector<TextMapEntry>;

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_TEXTMAP_HPP_ */
