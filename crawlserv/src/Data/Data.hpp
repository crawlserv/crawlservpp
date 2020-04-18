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
 * Data.hpp
 *
 * Custom data structures for database access by algorithms.
 *
 *  Created on: Jan 16, 2019
 *      Author: ans
 */

#ifndef DATA_DATA_HPP_
#define DATA_DATA_HPP_

#include <cstddef>	// std::size_t
#include <cstdint>	// std::int32_t, std::int64_t, std::uint32_t, std::uint64_t
#include <string>	// std::string
#include <tuple>	// std::tuple
#include <utility>	// std::pair
#include <vector>	// std::vector

namespace crawlservpp::Data {

	// data types
	enum Type {
		_unknown,
		_bool,
		_int32,
		_uint32,
		_int64,
		_uint64,
		_double,
		_string
	};

	// resolve std::size_t into appropriate data type
	template<int> Type getTypeOfSizeT();

	template<> inline Type getTypeOfSizeT<4>() {
		return Type::_uint32;
	}

	template<> inline Type getTypeOfSizeT<8>() {
		return Type::_uint64;
	}

	inline Type getTypeOfSizeT() {
		return getTypeOfSizeT<sizeof(std::size_t)>();
	}

	// generic value (can be numeric, null or a string)
	struct Value {
		// numeric value
		union {
			bool _b;
			std::int32_t _i32;
			std::uint32_t _ui32;
			std::int64_t _i64;
			std::uint64_t _ui64;
			double _d;
		};

		// string value
		std::string _s;

		// null value (if true; overwrites all other values)
		bool _isnull;

		// action that will be performed if a string is too large for the database (default: _error)
		enum _if_too_large {
			_error,	// throw a Database::Exception
			_trim,	// trim the string to an acceptable size
			_empty,	// use an empty string instead
			_null	// use a null value instead
		} _overflow;

		// constructors for initializing all kinds of generic values
		Value() { // null
			this->_ui64 = 0;
			this->_isnull = true;
			this->_overflow = _if_too_large::_error;
		}

		Value(bool value) { // bool
			this->_isnull = false;
			this->_overflow = _if_too_large::_error;
			this->_b = value;
		}

		Value(std::int32_t value) { // std::int32_t
			this->_isnull = false;
			this->_overflow = _if_too_large::_error;
			this->_i32 = value;
		}

		Value(std::uint32_t value) { // std::uint32_t
			this->_isnull = false;
			this->_overflow = _if_too_large::_error;
			this->_ui32 = value;
		}

		Value(std::int64_t value) { // std::int64_t
			this->_isnull = false;
			this->_overflow = _if_too_large::_error;
			this->_i64 = value;
		}

		Value(std::uint64_t value) { // std::uint64_t
			this->_isnull = false;
			this->_overflow = _if_too_large::_error;
			this->_ui64 = value;
		}

		Value(double value) { // double
			this->_isnull = false;
			this->_overflow = _if_too_large::_error;
			this->_d = value;
		}

		Value(const std::string& value) { // string
			this->_isnull = false;
			this->_overflow = _if_too_large::_error;
			this->_s = value;
		}

		// clear Value to null
		void clear() {
			Value();
		}
	};

	// structure for getting one value from a column
	struct GetValue {
		std::string table;
		std::string column;
		Type type;
		std::string condition;
		Value value;
	};

	// structure for getting multiple values of the same type from a column
	struct GetFields {
		std::string table;
		std::vector<std::string> columns;
		Type type;
		std::string condition;
		std::vector<Value> values;
	};

	// structure for getting multiple values of different types from a column
	struct GetFieldsMixed {
		std::string table;
		std::vector<std::pair<std::string, Type>> columns_types;
		std::string condition;
		std::vector<Value> values;
	};

	// structure for getting a a column
	struct GetColumn {
		std::string table;
		std::string column;
		Type type;
		std::string condition;	// optional
		std::string order;		// optional
		std::vector<Value> values;
	};

	// structure for getting multiple columns of the same type
	struct GetColumns {
		std::string table;
		std::vector<std::string> columns;
		Type type;
		std::string condition;	// optional
		std::string order;		// optional
		std::vector<std::vector<Value>> values;
	};

	// structure for getting multiple columns of different types
	struct GetColumnsMixed {
		std::string table;
		std::vector<std::pair<std::string, Type>> columns_types;
		std::string condition;	// optional
		std::string order;		// optional
		std::vector<std::vector<Value>> values;
	};

	// structure for inserting one value into a row
	struct InsertValue {
		std::string table;
		std::string column;
		Type type;
		Value value;
	};

	// structure for inserting multiple values of the same type into a row
	struct InsertFields {
		std::string table;
		std::vector<std::pair<std::string, Value>> columns_values;
		Type type;
	};

	// structure for inserting multiple values of different types into a row
	struct InsertFieldsMixed {
		std::string table;
		std::vector<std::tuple<std::string, Type, Value>> columns_types_values;
	};

	// structure for updating on value in a row
	struct UpdateValue {
		std::string table;
		std::string column;
		Type type;
		Value value;
		std::string condition;
	};

	// structure for updating multiple values of the same type in a row
	struct UpdateFields {
		std::string table;
		std::vector<std::pair<std::string, Value>> columns_values;
		Type type;
		std::string condition;
	};

	// structure for updating multiple values of different types in a row
	struct UpdateFieldsMixed {
		std::string table;
		std::vector<std::tuple<std::string, Type, Value>> columns_types_values;
		std::string condition;
	};

} /* crawlservpp::Data */

#endif /* DATA_DATA_HPP_ */
