/*
 * Data.h
 *
 * Custom data structures for database access by algorithms.
 *
 *  Created on: Jan 16, 2019
 *      Author: ans
 */

#ifndef MAIN_DATA_H_
#define MAIN_DATA_H_

#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace crawlservpp::Main::Data {

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
		unsigned long _ul;
		double _d;
	};

	// string value
	std::string _s;

	// null value (if true; overwrites all other values)
	bool _isnull;

	// action that will be performed if a string is too large for the database (default: _error)
	enum _if_too_large {
		_trim, // trim the string to an acceptable size
		_empty,// use an empty string instead
		_null, // use a null value instead
		_error // throw a std::runtime_exception
	} _overflow;

	// initializers for all types of generic values
	Value() { this->_ul = 0; this->_isnull = true; this->_overflow = _if_too_large::_error; } // null
	Value(bool value) { this->_isnull = false; this->_overflow = _if_too_large::_error; this->_b = value; } // bool
	Value(int value) { this->_isnull = false; this->_overflow = _if_too_large::_error; this->_i = value; } // int
	Value(unsigned int value) { this->_isnull = false; this->_overflow = _if_too_large::_error; this->_ui = value; } // unsigned int
	Value(long value) { this->_isnull = false; this->_overflow = _if_too_large::_error; this->_l = value; } // long
	Value(unsigned long value) { this->_isnull = false; this->_overflow = _if_too_large::_error; this->_ul = value; } // unsigned long
	Value(double value) { this->_isnull = false; this->_overflow = _if_too_large::_error; this->_d = value; } // double
	Value(const std::string& value) { this->_isnull = false; this->_overflow = _if_too_large::_error; this->_s = value; } // string
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

}

#endif /* MAIN_DATA_H_ */
