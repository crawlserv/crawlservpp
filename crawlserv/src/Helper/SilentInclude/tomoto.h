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
 * tomoto.h
 *
 * Silently include tomoto (at least in GCC).
 *
 *  Created on: Feb 6, 2021
 *      Author: ans
 */

#ifdef __GNUC__

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreorder"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"

#endif

#if _WIN64 || __x86_64__

#ifdef __SSE2__
#undef __SSE2__
#endif

#endif

#ifdef __clang__

#include <cstddef>		// std::size_t

/* fix clang casting bug in tomotopy */

namespace tomoto::serializer {

	template<std::size_t _len>
	struct Key;

	template<std::size_t _n>
	constexpr Key<_n> to_keyz(const char(&a)[_n]);

	template<typename T, std::size_t _n>
	constexpr Key<_n> to_keyz(const T(&a)[_n]);

} /* namespace tomoto::serializer */

#endif

#include "../../_extern/tomotopy/src/Labeling/FoRelevance.h"
#include "../../_extern/tomotopy/src/TopicModel/HDPModel.hpp"
#include "../../_extern/tomotopy/src/TopicModel/LDAModel.hpp"

#ifdef __GNUC__

#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop

#endif
