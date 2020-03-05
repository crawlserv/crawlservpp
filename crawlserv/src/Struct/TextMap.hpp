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
 * A text map used to annotate parts of a corpus (e.g. to store the dates or articles contained in these parts).
 *
 *  Created on: Mar 4, 2020
 *      Author: ans
 */

#ifndef STRUCT_TEXTMAP_HPP_
#define STRUCT_TEXTMAP_HPP_

#include <string>
#include <vector>

namespace crawlservpp::Struct {

	// structure for a part of the text map
	struct TextMapEntry {
		size_t pos;			// position of the annotated part inside the corpus
		size_t length;		// length of the annoted part inside the corpus
		std::string value;	// value of the annotation (e.g. article ID or date)

		// constructor setting default values
		TextMapEntry() : pos(0), length(0) {}

		// constructor setting partial custom values (i. e. an empty annotation)
		TextMapEntry(size_t setPos, size_t setLength) : pos(setPos), length(setLength) {}

		// constructor setting custom values
		TextMapEntry(
				size_t setPos,
				size_t setLength,
				const std::string& setValue
		) : pos(setPos), length(setLength), value(setValue) {}

		// reset values and free memory
		void free() {
			this->pos = 0;
			this->length = 0;

			std::string().swap(this->value);
		}
	};

	// for convenience
	using TextMap = std::vector<TextMapEntry>;

} /* crawlservpp::Struct */

#endif /* STRUCT_TEXTMAP_HPP_ */
