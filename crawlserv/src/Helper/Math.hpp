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

	///@name Mean, Median, and Standard Derivation
	///@{

	/*!
	 * Calculates the average (mean) of all elements in the given container.
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
				std::accumulate(values.begin(), values.end(), 0ULL) //NOLINT(bugprone-fold-init-type)
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

			return static_cast<R>((value1 + value2) / 2); //NOLINT(bugprone-integer-division)
		}

		const auto iterator{
			values.begin() + values.size() / 2
		};

		std::nth_element(values.begin(), iterator, values.end());

		return *iterator;
	}

	//! Calculates the variance from the given mean of all elements in the given container.
	/*!
	 * \param mean Mean of all elements in
	 *   the container.
	 * \param values Reference to the
	 *   container.
	 *
	 * \returns The variance, i.e. the
	 *   squared standard deviation, from
	 *   the given mean of all elements in
	 *   the given container.
	 *
	 */
	template<typename R, typename T, template<typename, typename> class Container>
		R variance(R mean, Container<T, std::allocator<T>>& values) {
		R sum{};

		for(const auto value : values) {
			sum += (value - mean) * (value - mean);
		}

		return sum / values.size();
	}

	/*!
	 * Calculates the variance of all elements in the given container.
	 *
	 * \param values Reference to the
	 *   container.
	 *
	 * \returns The variance, i.e. the
	 *   squared standard deviation, from
	 *   the mean of all elements in the
	 *   given container.
	 */
	template<typename R, typename T, template<typename, typename> class Container>
		R variance(Container<T, std::allocator<T>>& values) {
		const auto mean{avg(values)};

		return variance(mean, values);
	}

	///@}

} /* namespace crawlservpp::Helper::Math */

#endif /* HELPER_MATH_HPP_ */
