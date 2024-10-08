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
 * pipe.h
 *
 * Rename _popen and _pclose to popen and pclose on Windows.
 *
 *  Created on: May 14, 2019
 *      Author: ans
 */

#ifndef HELPER_PORTABILITY_PIPE_H_
#define HELPER_PORTABILITY_PIPE_H_

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

#endif /* HELPER_PORTABILITY_PIPE_H_ */
