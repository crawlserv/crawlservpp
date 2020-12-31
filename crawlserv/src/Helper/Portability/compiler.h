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
 * compiler.h
 *
 * Check whether the current compiler and compiler version are supported.
 *
 *  Created on: May 15, 2019
 *      Author: ans
 */

#ifndef HELPER_PORTABILITY_COMPILER_H_
#define HELPER_PORTABILITY_COMPILER_H_

// check compiler version
#if defined(__clang__)
#if __clang_major__ < 5
#warning "Compiler version is not supported."
#endif
#elif defined(__GNUC__) || defined(__GNUG__)
#if __GNUC__ < 7
#warning "Compiler version is not supported."
#endif
#elif defined(_MSC_VER)
#warning "Compiler is not supported."
#endif

#endif /* HELPER_PORTABILITY_COMPILER_H_ */
