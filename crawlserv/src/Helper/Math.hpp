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
 * Math.hpp
 *
 * Namespace for global math functions.
 *
 *  Created on: Nov 13, 2020
 *      Author: ans
 */

#ifndef HELPER_MATH_HPP_
#define HELPER_MATH_HPP_

#include <algorithm>	// std::nth_element
#include <cstddef>		// std::size_t
#include <memory>		// std::allocator
#include <numeric>		// std::accumulate
#include <vector>		// std::vector

//! Namespace for global math functions.
namespace crawlservpp::Helper::Math {

	///@name Average and Median
	///@{

	/*!
	 * Calculates the average of all elements in the given container.
	 *
	 * \param values Constant reference to
	 *   the container.
	 *
	 * \returns The average of all elements
	 *   in the given container
	 */
	template<typename R, typename T, template<typename, typename> class Container>
	R avg(const Container<T, std::allocator<T>>& values) {
		return static_cast<R>(
				std::accumulate(values.begin(), values.end(), 0LL)
		) / values.size();
	}

	/*!
	 * Calculates the median of all elements in the given container.
	 *
	 * \param values Reference to the
	 *   container.
	 *
	 * \returns The median of all elements
	 *   in the given container.
	 */
	template<typename R, typename T, template<typename, typename> class Container>
	R median(Container<T, std::allocator<T>>& values) {
		if(values.empty()) {
			return R{};
		}

		if(values.size() % 2 == 0) {
			const auto iterator1{
				values.begin() + values.size() / 2 - 1
			};
			const auto iterator2{
				values.begin() + values.size() / 2
			};

			std::nth_element(values.begin(), iterator1 , values.end());

			const auto value1{*iterator1};

			std::nth_element(values.begin(), iterator2 , values.end());

			const auto value2{*iterator2};

			return (value1 + value2) / 2;
		}
		else {
			const auto iterator{
				values.begin() + values.size() / 2
			};

			std::nth_element(values.begin(), iterator, values.end());

			return *iterator;
		}
	}

	///@}

} /* namespace crawlservpp::Helper::Math */

#endif /* HELPER_MATH_HPP_ */
