/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
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

#include <cstddef>	// std::size_t
#include <string>	// std::string
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
		std::size_t pos{0};

		//! The length of the annotated part inside the text.
		std::size_t length{0};

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
		) : pos(setPos), length(setLength) {}

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
		) : pos(setPos), length(setLength), value(setValue) {}

		///@}
		///@name Cleanup
		///@{

		//! Resets its properties to their default values and frees the memory used by the entry.
		void free() {
			this->pos = 0;
			this->length = 0;

			std::string().swap(this->value);
		}

		///@}
	};

	//! A text map is defined as a vector of text map entries.
	using TextMap = std::vector<TextMapEntry>;

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_TEXTMAP_HPP_ */
