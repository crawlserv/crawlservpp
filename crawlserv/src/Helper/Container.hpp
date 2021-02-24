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
 * Container.hpp
 *
 * Helper function templates for container operations.
 *
 *  Created on: Feb 24, 2021
 *      Author: ans
 */

#ifndef HELPER_CONTAINER_HPP_
#define HELPER_CONTAINER_HPP_

#include <iterator>	// std::begin, std::end, std::make_move_iterator
#include <numeric>	// std::accumulate

//! Namespace for global container helper function templates.
namespace crawlservpp::Helper::Container {

	//! Appends (part of) an iterable container to another container.
	/*!
	 * \param to The container to append
	 *   elements to.
	 * \param from The container to append
	 *   elements from.
	 * \param startAt The first element in
	 *   \p from to append.
	 * \param endAt The last element in \p from
	 *   to appemd.
	 */
	template<typename T> static void append(
			T& to,
			const T& from,
			typename T::size_type startAt,
			typename T::size_type endAt
	) {
		to.insert(std::end(to), std::begin(from) + startAt, std::begin(from) + endAt);
	}

	//! Appends (part of) an iterable container to another iterable container.
	/*!
	 * \param to The container to append
	 *   elements to.
	 * \param from The container to append
	 *   elements from.
	 * \param startAt The first element in
	 *   \p from to append.
	 */
	template<typename T> static void append(
			T& to,
			const T& from,
			typename T::size_type startAt = 0
	) {
		to.insert(std::end(to), std::begin(from) + startAt, std::end(from));
	}

	//! Moves the elements of an iterable container into another iterable container.
	/*!
	 * Appends the elements at the end of the
	 *  target container.
	 *
	 * \param to The container to move into.
	 * \param from The container to move from.
	 */
	template<typename T> static void moveInto(T& to, T& from) {
		to.insert(
				std::end(to),
				std::make_move_iterator(std::begin(from)),
				std::make_move_iterator(std::end(from))
		);
	}

	//! Moves the elements of an iterable container into another iterable container.
	/*!
	 * Inserts the elements at the given
	 *  position.
	 *
	 * \param to The container to move into.
	 * \param from The container to move from.
	 * \param at The position at which the new
	 *   elements will be inserted into \p to.
	 */
	template<typename T> static void moveInto(T& to, T& from, typename T::size_type at) {
		to.insert(
				std::begin(to) + at,
				std::make_move_iterator(std::begin(from)),
				std::make_move_iterator(std::end(from))
		);
	}

	//! Erases the first elements of an iterable container.
	/*!
	 * \param container The container to erase
	 *   elements from.
	 * \param n The number of elements to be
	 *   erased from the beginning of the
	 *   container.
	 */
	template<typename T> static void eraseFirst(T& container, typename T::size_type n) {
		container.erase(std::begin(container), std::begin(container) + n);
	}

	//! Returns the number of bytes in an iterable container.
	/*!
	 * \note The type of the elements in the
	 *  container must provide a \c size() member
	 *  function, returning the number of bytes
	 *  in the element
	 */
	template<typename T> static typename T::size_type bytes(const T& container) {
		return std::accumulate(
				std::begin(container),
				std::end(container),
				typename T::size_type{},
				[](const auto size, const auto& element) {
					return size + element.size();
				}
		);
	}

} /* namespace crawlservpp::Helper::Container */

#endif /* HELPER_CONTAINER_HPP_ */
