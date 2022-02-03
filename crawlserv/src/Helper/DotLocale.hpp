/*
 *
 * ---
 *
 *  Copyright (C) 2022 Anselm Schmidt (ans[ät]ohai.su)
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
 * DotLocale.hpp
 *
 * Locale using dots as decimal separators.
 *
 *  Created on: Aug 20, 2021
 *      Author: ans
 */

#ifndef DOTLOCALE_HPP_
#define DOTLOCALE_HPP_

#include <locale>	// std::locale, std::numpunct

namespace crawlservpp::Helper {

	class DotLocale : public std::numpunct<char> {
	public:
		DotLocale() = default;
		virtual ~DotLocale() = default;

		static inline std::locale locale() {
			return std::locale{std::locale(), new DotLocale()};
		}

	protected:
		virtual char do_decimal_point() const {
			return '.';
		}
	};

} /* namespace crawlservpp::Helper */

#endif /* DOTLOCALE_HPP_ */
