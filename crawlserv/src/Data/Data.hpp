/*
 *
 * ---
 *
 *  Copyright (C) 2019-2020 Anselm Schmidt (ans[ät]ohai.su)
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
#include <string>	// std::string
#include <tuple>	// std::tuple
#include <utility>	// std::pair
#include <vector>	// std:vector

namespace crawlservpp::Data {

	// data types
	enum Type {
		_unknown,
		_bool,
		_int,
		_uint,
		_long,
		_ulong,
		_double,
		_string
	};

	// generic value (can be numeric, null or a string)
	struct Value {
		// numeric value
		union {
			bool _b;
			int _i;
			unsigned int _ui;
			long _l;
			std::size_t _ul;
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

		// initializers for all types of generic values
		Value() { this->_ul = 0; this->_isnull = true; this->_overflow = _if_too_large::_error; } // null
		Value(bool value) { this->_isnull = false; this->_overflow = _if_too_large::_error; this->_b = value; } // bool
		Value(int value) { this->_isnull = false; this->_overflow = _if_too_large::_error; this->_i = value; } // int
		Value(unsigned int value) { this->_isnull = false; this->_overflow = _if_too_large::_error; this->_ui = value; } // unsigned int
		Value(long value) { this->_isnull = false; this->_overflow = _if_too_large::_error; this->_l = value; } // long
		Value(std::size_t value) { this->_isnull = false; this->_overflow = _if_too_large::_error; this->_ul = value; } // std::size_t
		Value(double value) { this->_isnull = false; this->_overflow = _if_too_large::_error; this->_d = value; } // double
		Value(const std::string& value) { this->_isnull = false; this->_overflow = _if_too_large::_error; this->_s = value; } // string
		void clear() { Value(); }
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
