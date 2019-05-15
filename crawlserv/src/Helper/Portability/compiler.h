/*
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
#if defined(__CLANG__)
#if __clang_major__ < 6
#warning "Compiler version is not supported."
#endif
#elif defined(__GNUC__) || defined(__GNUG__)
#if __GNUC__ < 6
#warning "Compiler version is not supported."
#endif
#elif defined(_MSC_VER)
#warning "Compiler is not supported."
#endif

#endif /* HELPER_PORTABILITY_COMPILER_H_ */
