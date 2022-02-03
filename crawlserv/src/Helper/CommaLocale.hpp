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
 * CommaLocale.hpp
 *
 * Locale using commas as thousands separators.
 *
 *  Created on: Aug 14, 2021
 *      Author: ans
 */

#ifndef COMMALOCALE_HPP_
#define COMMALOCALE_HPP_

#include <locale>	// std::locale, std::numpunct
#include <string>	// std::string

namespace crawlservpp::Helper {

	class CommaLocale : public std::numpunct<char> {
	public:
		CommaLocale() = default;
		virtual ~CommaLocale() = default;

		static inline std::locale locale() {
			return std::locale{std::locale(), new CommaLocale()};
		}

	protected:
		virtual char do_thousands_sep() const {
			return ',';
		}

		virtual std::string do_grouping() const {
			return "\03";
		}
	};

} /* namespace crawlservpp::Helper */

#endif /* COMMALOCALE_HPP_ */
