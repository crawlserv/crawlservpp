
/* 
 * 
 * ---
 * 
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
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
 * style.js
 * 
 * JavaScript styling for the PHP/JavaScript crawlserv++ frontend.
 * 
 */

// alternate menu items and content blocks
window.onload = function() {
	var menuitems = document.getElementsByClassName("menu-item");
	var i = 0;
	
	for (i = 0; i < menuitems.length; i++)
		if((i + 1) % 2 == 0) menuitems[i].classList.add("menu-item-alternate");
	
	if($("#license").length)
		$("#license").height($("#license")[0].scrollHeight);
}