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
 * ConfigItem.hpp
 *
 * Configuration item (category, name and JSON value).
 *
 *  Created on: Mar 9, 2019
 *      Author: ans
 */

#ifndef STRUCT_CONFIGITEM_HPP_
#define STRUCT_CONFIGITEM_HPP_

#include "../_extern/rapidjson/include/rapidjson/document.h"

#include <string>	// std::string

namespace crawlservpp::Struct {

	//! Configuration item containing its category, name, and JSON value.
	struct ConfigItem {
		///@name Properties
		///@{

		//! Category of the configuration item.
		std::string category;

		//! Name of the configuration item.
		std::string name;

		//! JSON value of the configuration item, or @c nullptr if not initialized.
		const rapidjson::Value * value{nullptr};

		///@}
		///@name Getter
		///@{

		//! Combines category and name into one string.
		/*!
		 * \returns New std::string containing both category and name,
		 *   separated by a dot, i.e. @c category.name.
		 */
		[[nodiscard]] std::string str() const {
			return std::string(category + "." + name);
		}

		///@}
		///@name Construction and Destruction
		///@{

		//! Default constructor.
		ConfigItem() = default;

		//! Default destructor.
		virtual ~ConfigItem() = default;

		///@}
		/**@name Copy and Move
		 * The class is not copyable, only (default) moveable.
		 */
		///@{

		//! Deleted copy constructor.
		ConfigItem(ConfigItem&) = delete;

		//! Deleted copy assignment operator.
		ConfigItem& operator=(ConfigItem&) = delete;

		//! Default move constructor.
		ConfigItem(ConfigItem&&) = default;

		//! Default move assignment operator.
		ConfigItem& operator=(ConfigItem&&) = default;

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_CONFIGITEM_HPP_ */
