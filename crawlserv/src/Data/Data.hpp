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
 * Data.hpp
 *
 * Custom data structures for database access by algorithms.
 *
 *  Created on: Jan 16, 2019
 *      Author: ans
 */

#ifndef DATA_DATA_HPP_
#define DATA_DATA_HPP_

#include <algorithm>	// std::transform
#include <array>		// std::array
#include <cctype>		// std::tolower
#include <cstddef>		// std::size_t
#include <cstdint>		// std::int32_t, std::int64_t, std::uint32_t, std::uint64_t
#include <string>		// std::string
#include <tuple>		// std::tuple
#include <utility>		// std::pair, std::swap
#include <vector>		// std::vector

namespace crawlservpp::Data {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! The number of bytes of a 32-bit value.
	inline constexpr auto bytes32bit{4};

	//! The number of bytes of a 64-bit value.
	inline constexpr auto bytes64bit{8};

	///@}

	/*
	 * DECLARATIONS
	 */

	//! Data types.
	enum Type {
		//! Unknown data type.
		_unknown,

		//! Boolean value.
		_bool,

		//! 32-bit integer.
		_int32,

		//! Unsigned 32-bit integer.
		_uint32,

		//! 64-bit integer.
		_int64,

		//! Unsigned 64-bit integer.
		_uint64,

		//! Floating point value (with double precision).
		_double,

		//! String.
		_string
	};

	//! A generic value.
	/*!
	 * The value can be boolean, numeric, null or a string.
	 */
	struct Value { //NOLINT(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
		//! Union for numeric (including boolean) values.
		union {
			//! Boolean value.
			bool _b;

			//! 32-bit integer value.
			std::int32_t _i32;

			//! Unsigned 32-bit integer value.
			std::uint32_t _ui32;

			//! 64-bit integer value.
			std::int64_t _i64;

			//! Unsigned 64-bit integer value.
			std::uint64_t _ui64{};

			//! Floating point value (with double precision).
			double _d;
		};

		//! String value.
		std::string _s;

		//! Null value.
		bool _isnull{true};

		//! Enumeration for the action that will be performed if a string is too large for the database.
		enum _if_too_large {
			//! Throw a Database::Exception
			_error,

			//! Trim the string to an acceptable size.
			_trim,

			//! Use an empty string instead.
			_empty,

			//! Use a null value instead.
			_null

		}

		//! Action that will be performed if a string is too large for the database.
		/*!
		 * On default, a Database::Exception will be thrown.
		 */
		_overflow{_if_too_large::_error};

		//! Default constructor initializing a null value.
		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
		Value() = default;

		//! Constructor initializing a boolean value.
		/*!
		 * \param value The boolean value to be set.
		 */
		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
		explicit Value(bool value) : _b(value), _isnull(false) {}

		//! Constructor initializing a 32-bit integer.
		/*!
		 * \param value The 32-bit integer value to be set.
		 */
		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
		explicit Value(std::int32_t value) : _i32(value), _isnull(false) {}

		//! Constructor initializing an unsigned 32-bit integer.
		/*!
		 * \param value The unsigned 32-bit integer value to be set.
		 */
		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
		explicit Value(std::uint32_t value) : _ui32(value), _isnull(false) {}

		//! Constructor initializing a 64-bit integer.
		/*!
		 * \param value The 64-bit integer value to be set.
		 */
		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
		explicit Value(std::int64_t value) : _i64(value), _isnull(false) {}

		//! Constructor initializing an unsigned 64-bit integer.
		/*!
		 * \param value The unsigned 64-bit integer value to be set.
		 */
		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
		explicit Value(std::uint64_t value) : _ui64(value), _isnull(false) {}

		//! Constructor initializing a floating point value (with double precision).
		/*!
		 * \param value The floating point value to be set.
		 */
		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
		explicit Value(double value) : _d(value), _isnull(false) {}

		//! Constructor initializing a string value.
		/*!
		 * \param value Constant reference to a string containing
		 *   the string value to be set.
		 */
		//NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
		explicit Value(const std::string& value) : _s(value), _isnull(false) {}

		//! Clears the current value and re-initializes it as null value.
		void clear() {
			*this = Value();
		}
	};

	//! Structure for retrieving one value from a table column.
	struct GetValue {
		//! The name of the table.
		std::string table;

		//! The name of the column.
		std::string column;

		//! The data type of the value to be retrieved.
		Type type{_unknown};

		//! Condition to be added to the SQL query retrieving the value.
		std::string condition;

		//! The retrieved value.
		Value value;
	};

	//! Structure for retrieving multiple values of the same type from a table column.
	struct GetFields {
		//! The name of the table.
		std::string table;

		//! Vector containing the names of the columns to be retrieved.
		std::vector<std::string> columns;

		//! The data type of the values to be retrieved.
		Type type{_unknown};

		//! Condition to be added to the SQL query retrieving the values.
		std::string condition;

		//! Vector containing the retrieved values.
		std::vector<Value> values;
	};

	//! Structure for getting multiple values of different types from a table column.
	struct GetFieldsMixed {
		//! The name of the table.
		std::string table;

		//! Vector containing the names and data types of the columns to be retrieved.
		std::vector<std::pair<std::string, Type>> columns_types;

		//! Condition to be added to the SQL query retrieving the values.
		std::string condition;

		//! Vector containing the retrieved values.
		std::vector<Value> values;
	};

	//! Structure for retrieving the values in a table column.
	struct GetColumn {
		//! The name of the table.
		std::string table;

		//! The name of the column.
		std::string column;

		//! The data type of the column.
		Type type{_unknown};

		//! Optional condition to be added to the SQL query retrieving the values of the column.
		std::string condition;

		//! Optional order to be applied to the SQL query retrieving the values of the column.
		/*!
		 * The values will be ordered by the given
		 *  columns, in the same order as they are
		 *  present in the vector.
		 */
		std::vector<std::string> order;

		//! Vector containing the retrieved values.
		std::vector<Value> values;
	};

	//! Structure for retrieving multiple table columns of the same type.
	struct GetColumns {
		//! The name of the table.
		std::string table;

		//! Vector containing the names of the columns.
		std::vector<std::string> columns;

		//! The data type of the columns.
		Type type{_unknown};

		//! Optional condition to be added to the SQL query retrieving the values of the columns.
		std::string condition;

		//! Optional order to be applied to the SQL query retrieving the values of the columns.
		/*!
		 * The values will be ordered by the given
		 *  columns, in the same order as they are
		 *  present in the vector.
		 */
		std::vector<std::string> order;

		//! Vector containing the retrieved columns as vectors of the retrieved values.
		std::vector<std::vector<Value>> values;
	};

	//! Structure for retrieving multiple table columns of different types.
	struct GetColumnsMixed {
		//! The name of the table.
		std::string table;

		//! Vector containing the names and data types of the columns to be retrieved.
		std::vector<std::pair<std::string, Type>> columns_types;

		//! Optional condition to be added to the SQL query retrieving the values of the columns.
		std::string condition;

		//! Optional order to be applied to the SQL query retrieving the values of the columns.
		/*!
		 * The values will be ordered by the given
		 *  columns, in the same order as they are
		 *  present in the vector.
		 */
		std::vector<std::string> order;

		//! Vector containing the retrieved columns as vectors of the retrieved values.
		std::vector<std::vector<Value>> values;
	};

	//! Structure for inserting one value into a table.
	struct InsertValue {
		//! The name of the table.
		std::string table;

		//! The name of the column.
		std::string column;

		//! The data type of the value.
		Type type{_unknown};

		//! The value to be inserted.
		Value value;
	};

	//! Structure for inserting multiple values of the same type into a table.
	struct InsertFields {
		//! The name of the table.
		std::string table;

		//! Vector containing the names of the columns and the values to be inserted into them.
		std::vector<std::pair<std::string, Value>> columns_values;

		//! The data type of the values.
		Type type{_unknown};
	};

	//! Structure for inserting multiple values of different types into a row.
	struct InsertFieldsMixed {
		//! The name of the table.
		std::string table;

		//! Vector containing the names of the columns, their data types and the values to be inserted into them.
		std::vector<std::tuple<std::string, Type, Value>> columns_types_values;
	};

	//! Structure for updating one value in a table.
	struct UpdateValue {
		//! The name of the table.
		std::string table;

		//! The name of the column.
		std::string column;

		//! The data type of the value.
		Type type{_unknown};

		//! The new value to be set.
		/*!
		 * The old value will be overwritten.
		 */
		Value value;

		//! The condition to be added to the SQL query updating the value.
		std::string condition;
	};

	//! Structure for updating multiple values of the same type in a table.
	struct UpdateFields {
		//! The name of the table.
		std::string table;

		//! Vector containing the names of the columns to be updated and the new values to be set.
		/*!
		 * The old values will be overwritten.
		 */
		std::vector<std::pair<std::string, Value>> columns_values;

		//! The data type of the values.
		Type type{_unknown};

		//! The condition to be added to the SQL query updating the value.
		std::string condition;
	};

	//! Structure for updating multiple values of different types in a table.
	struct UpdateFieldsMixed {
		//! The name of the table.
		std::string table;

		//! Vector containing the names of the columns to be updated, their data types and the new values to be set.
		/*!
		 * The old values will be overwritten.
		 */
		std::vector<std::tuple<std::string, Type, Value>> columns_types_values;

		//! The condition to be added to the SQL query updating the value.
		std::string condition;
	};

	/*
	 * HELPER FUNCTION
	 */

	///@name Helper Function
	///@{

	//! Parses the given SQL data type.
	/*!
	 * \param sqlType Constant reference to a string
	 *   containing the SQL data type to parse.
	 *
	 * \returns The parsed data type.
	 *
	 * \sa Type
	 */
	[[nodiscard]] inline Type parseSQLType(std::string sqlType) {
		// constants classifying SQL types
		constexpr std::array boolTypes{"bool", "boolean"};
		constexpr std::array int32Types{"bit", "tinyint", "smallint", "mediumint", "int", "integer"};
		constexpr std::array int64Types{"bigint"};
		constexpr std::array doubleTypes{"float", "double", "double precision", "decimal", "dec"};
		constexpr std::array stringTypes{
			"char",
			"varchar",
			"binary",
			"varbinary",
			"tinyblob",
			"tinytext",
			"text",
			"blob",
			"mediumtext",
			"longtext",
			"longblob",
			"enum",
			"set"
		};

		// convert data type to lower case
		std::transform(
				sqlType.begin(),
				sqlType.end(),
				sqlType.begin(),
				[](const auto c) {
					return std::tolower(c);
				}
		);

		// remove size from retrieved data type
		const auto bracketPos{sqlType.find_first_of('(')};

		if(bracketPos != std::string::npos) {
			const auto bracketEnd{sqlType.find_first_of(')', bracketPos + 1)};

			if(bracketEnd != std::string::npos) {
				sqlType = sqlType.substr(0, bracketPos) + sqlType.substr(bracketEnd + 1);
			}
		}

		// detect types (unsigned ones first)
		for(const auto& type : boolTypes) {
			const std::string typeString{type};

			if(sqlType.substr(0, typeString.length()) == typeString) {
				return Type::_bool;
			}
		}

		for(const auto& type : int32Types) {
			const std::string typeString{type};
			const auto unsignedType{typeString + " unsigned"};

			if(sqlType.substr(0, unsignedType.length()) == unsignedType) {
				return Type::_uint32;
			}

			if(sqlType.substr(0, typeString.length()) == typeString) {
				return Type::_int32;
			}
		}

		for(const auto& type : int64Types) {
			const std::string typeString{type};
			const auto unsignedType{typeString + " unsigned"};

			if(sqlType.substr(0, unsignedType.length()) == unsignedType) {
				return Type::_uint64;
			}

			if(sqlType.substr(0, typeString.length()) == typeString) {
				return Type::_int64;
			}
		}

		for(const auto& type : doubleTypes) {
			const std::string typeString{type};

			if(sqlType.substr(0, typeString.length()) == typeString) {
				return Type::_double;
			}
		}

		for(const auto& type : stringTypes) {
			const std::string typeString{type};

			if(sqlType.substr(0, typeString.length()) == typeString) {
				return Type::_string;
			}
		}

		return Type::_unknown;
	}

	///@}

	/*
	 * TEMPLATE FUNCTIONS
	 */

	///@name Template Functions
	///@{

	template<int> Type getTypeOfSizeT();

	//! Identifies std::size_t as a 32-bit integer.
	template<> inline Type getTypeOfSizeT<bytes32bit>() {
		return Type::_uint32;
	}

	//! Identifies std::size_t as a 64-bit integer.
	template<> inline Type getTypeOfSizeT<bytes64bit>() {
		return Type::_uint64;
	}

	//! Resolves std::size_t into the appropriate data type.
	inline Type getTypeOfSizeT() {
		return getTypeOfSizeT<sizeof(std::size_t)>();
	}

	///@}

} /* namespace crawlservpp::Data */

#endif /* DATA_DATA_HPP_ */
