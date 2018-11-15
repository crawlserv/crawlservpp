// alternate menu items
window.onload = function() {
	var menuitems = document.getElementsByClassName("menu-item");
	var i = 0;
	for (i = 0; i < menuitems.length; i++) {
		if((i + 1) % 2 == 0) menuitems[i].classList.add("menu-item-alternate");
	}
}