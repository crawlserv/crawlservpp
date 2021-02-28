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
 * AspellConfig.hpp
 *
 * RAII wrapper for pointer to aspell configurations.
 *  Does NOT have ownership of the pointer, but takes care of its deletion!
 *
 *  Created on: Feb 27, 2021
 *      Author: ans
 */

#ifndef WRAPPER_ASPELLCONFIG_HPP_
#define WRAPPER_ASPELLCONFIG_HPP_

#include "../Main/Exception.hpp"

#include <aspell.h>

#include <string>	// std::string, std::to_string

namespace crawlservpp::Wrapper {

	/*
	 * DECLARATION
	 */

	//! RAII wrapper for @c aspell configurations.
	/*!
	 * Creates the configuration on
	 *  construction and deletes it on
	 *  destruction, if still necessary,
	 *  avoiding memory leaks.
	 *
	 * \note The class does not own the
	 *   underlying pointer, but takes care
	 *   of its deletion via API call.
	 */
	class AspellConfig {
	public:
		///@name Construction and Destruction
		///@{

		AspellConfig();
		virtual ~AspellConfig();

		///@}
		///@name Getters
		///@{

		[[nodiscard]] ::AspellConfig * get();
		[[nodiscard]] const ::AspellConfig * getc() const;
		[[nodiscard]] bool valid() const;

		///@}
		///@name Setter
		///@{

		void setOption(const std::string& name, const std::string& value);

		///@}
		///@name Cleanup
		///@{

		void clear();

		///@}
		/**@name Copy and Move
		 * The class is both copyable and
		 *  moveable.
		 */
		///@{

		AspellConfig(const AspellConfig& other);
		AspellConfig(AspellConfig&& other) noexcept;
		AspellConfig& operator=(const AspellConfig& other);
		AspellConfig& operator=(AspellConfig&& other) noexcept;

		///@}

		//! Class for @c aspell configuration-specific exceptions.
		MAIN_EXCEPTION_CLASS();

	private:
		// underlying pointer to the configuration
		::AspellConfig * ptr{nullptr};
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * CONSTRUCTION AND DESTRUCTION
	 */

	//! Constructor creating a new configuration.
	inline AspellConfig::AspellConfig() {
		this->ptr = new_aspell_config();
	}

	//! Destructor deleting the configuration, if necessary.
	/*!
	 * \sa clear
	 */
	inline AspellConfig::~AspellConfig() {
		this->clear();
	}

	/*
	 * GETTERS
	 */

	//! Gets a pointer to the underlying configuration.
	/*!
	 * \returns A pointer to the underlying
	 *   configuration or @c nullptr if the
	 *   configuration is not valid.
	 *
	 * \sa getc, valid
	 */
	inline ::AspellConfig * AspellConfig::get() {
		return this->ptr;
	}

	//! Gets a constant pointer to the underlying configuration.
	/*!
	 * \returns A constant pointer to the
	 *   underlying configuration or
	 *   @c nullptr if the configuration is
	 *   not valid.
	 *
	 * \sa get, valid
	 */
	inline const ::AspellConfig * AspellConfig::getc() const {
		return this->ptr;
	}

	//! Gets whether the configuration is valid.
	/*!
	 * \returns True, if the configuration
	 *   is valid. False otherwise.
	 */
	inline bool AspellConfig::valid() const {
		return this->ptr != nullptr;
	}

	/*
	 * SETTER
	 */

	//! Sets an option in the configuration.
	/*!
	 * The previous value of the option
	 *  will be overwritten.
	 *
	 * \param name Name of the @c aspell
	 *   configuration option.
	 * \param value New value of the option.
	 *
	 * \throws AspellConfig::Exception if
	 *   the current configuration, the name
	 *   or the value of the option are not
	 *   valid, or if its value could not be
	 *   changed.
	 */
	inline void AspellConfig::setOption(const std::string& name, const std::string& value) {
		if(this->ptr == nullptr) {
			throw Exception(
					"AspellConfig::setOption():"
					" The configuration is not valid"
			);
		}

		if(aspell_config_replace(this->ptr, name.c_str(), value.c_str()) == 0) {
			throw Exception(
					"AspellConfig::setOption():"
					" Aspell error #"
					+ std::to_string(aspell_config_error_number(this->ptr))
					+ ": "
					+ aspell_config_error_message(this->ptr)
			);
		}
	}

	/*
	 * CLEANUP
	 */

	//! Deletes the configuration, if necessary.
	inline void AspellConfig::clear() {
		if(this->ptr != nullptr) {
			delete_aspell_config(this->ptr);

			this->ptr = nullptr;
		}
	}

	/*
	 * COPY AND MOVE
	 */

	//! Copy constructor.
	/*!
	 * Creates a copy of the underlying
	 *  configuration in the given
	 *  instance, saving it in this
	 *  instance.
	 *
	 * If the other configuration is
	 *  invalid, the current instance will
	 *  also be invalid.
	 *
	 * \param other The configuration to
	 *   copy from.
	 */
	inline AspellConfig::AspellConfig(const AspellConfig& other) {
		if(other.ptr != nullptr) {
			aspell_config_assign(this->ptr, other.ptr);
		}
	}

	//! Move constructor.
	/*!
	 * Moves the configuration from the
	 *  specified location into this
	 *  instance of the class.
	 *
	 * \note The other configuration will
	 *   be invalidated by this move.
	 *
	 * \param other The configuration to
	 *   move from.
	 *
	 * \sa valid
	 */
	inline AspellConfig::AspellConfig(AspellConfig&& other) noexcept : ptr(other.ptr) {
		other.ptr = nullptr;
	}

	//! Copy assignment operator.
	/*!
	 * Clears the existing configuration
	 *  if necessary and creates a copy of
	 *  the underlying configuration in
	 *  the given instance, saving it in
	 *  this instance.
	 *
	 * \note Nothing will be done if used
	 *   on itself.
	 *
	 * \param other The configuration
	 *   to copy from.
	 *
	 * \returns A reference to the class
	 *   containing the copy of the
	 *   configuration (i.e. @c *this).
	 */
	inline AspellConfig& AspellConfig::operator=(const AspellConfig& other) {
		if(&other != this) {
			this->clear();

			aspell_config_assign(this->ptr, other.ptr);
		}

		return *this;
	}

	//! Move assignment operator.
	/*!
	 * Moves the configuration from the
	 *  specified location into this
	 *  instance of the class.
	 *
	 * \note The other configuration will
	 *   be invalidated by this move.
	 *
	 * \note Nothing will be done if used on
	 *   itself.
	 *
	 * \param other The configuration to
	 *   move from.
	 *
	 * \returns A reference to the instance
	 *   containing the configuration after
	 *   moving (i.e. @c *this).
	 *
	 * \sa valid
	 */
	inline AspellConfig& AspellConfig::operator=(AspellConfig&& other) noexcept {
		if(&other != this) {
			this->clear();

			this->ptr = other.ptr;

			other.ptr = nullptr;
		}

		return *this;
	}

} /* namespace crawlservpp::Wrapper */

#endif /* WRAPPER_ASPELLCONFIG_HPP_ */
