/*
 * Configuration class for the crawlserv frontend
 * 
 */

// helper function to flatten an object
var flattenObject = function(ob) {
    var toReturn = {};

    for (var i in ob) {
        if (!ob.hasOwnProperty(i)) continue;

        if ((typeof ob[i]) == "object") {
            var flatObject = flattenObject(ob[i]);
            for (var x in flatObject) {
                if (!flatObject.hasOwnProperty(x)) continue;

                toReturn[i + '.' + x] = flatObject[x];
            }
        } else {
            toReturn[i] = ob[i];
        }
    }
    return toReturn;
};

class Config {
	
	/*
	 * GENERAL FUNCTIONS
	 */
	
	// constructor: create or load configuration
	constructor(module, json, mode) {		
		// link class to view
		this.view = $("#config-view");
		
		if(this.view) {
			// set empty content
			this.content = "";
			
			// parse data structure from JSON file for module
			let thisClass = this;
			var startTime = +new Date();
			$.getJSON("json/" + module + ".json", function(data) {
				thisClass.config_base = data;
				
				// check data structure for consistency (only one id per
				// category)
				for(var cat in thisClass.config_base) {
					var cat_ids = [];
					for(var opt in thisClass.config_base[cat]) {
						// ignore category properties
						if(thisClass.config_base[cat].hasOwnProperty(opt) && opt != "id" && opt != "open" && opt != "label") {
							let id = thisClass.config_base[cat][opt].id;
							if(cat_ids.includes(id)) {
								alert("Could not load data structure.\n\nDuplicate id #" + id + " in category \'" + cat
										+ "\', option \'" + opt + "\'.");
								return;
							}
							else cat_ids.push(id);
						}
					}
				}
				
				var endTime = +new Date();
				console.log("Config(): Data structure loaded after " + msToStr(endTime - startTime) + ".");
				startTime = endTime;
				
				// parse configuration from database
				thisClass.config_db = db_config;
				
				// parse current configuration
				thisClass.config_current = $.parseJSON(json);
				
				// combine configurations
				thisClass.config_combined = [];
				for(var cat in thisClass.config_base) {
					for(var opt in thisClass.config_base[cat]) {
						// ignore category properties
						if(thisClass.config_base[cat].hasOwnProperty(opt) && opt != "id" && opt != "open" && opt != "label") {
							var optobj = {};
							optobj.cat = cat;
							optobj.name = opt;
							optobj.value = thisClass.config_base[cat][opt].default;
							
							// overwrite value from database
							for(var i = 0; i < thisClass.config_db.length; i++) {
								if(thisClass.config_db[i].cat == cat && thisClass.config_db[i].name == opt) {
									optobj.value = thisClass.config_db[i].value;
								}
							}
							
							// overwrite value from current configuration
							for(var i = 0; i < thisClass.config_current.length; i++) {
								if(thisClass.config_current[i].cat == cat && thisClass.config_current[i].name == opt) {
									optobj.value = thisClass.config_current[i].value;
								}			
							}
							
							thisClass.config_combined.push(optobj);
						}
					}
				}				
				
				// create configuration content
				for(var cat in thisClass.config_base) {
					var simple = [];
					// start category
					let cobj = thisClass.config_base[cat];
					thisClass.content += thisClass.startCat(cobj.id, cat, cobj.label, cobj.open);
					
					// go through all options in catgeory
					for(var opt in thisClass.config_base[cat]) {
						// ignore category (and internal JavaScript) properties
						if(cobj.hasOwnProperty(opt) && opt != "id" && opt != "open" && opt != "label") {
							// set current object
							let obj = cobj[opt];
							
							// get value
							var value = thisClass.getConfValue(cat, opt);
							
							// show option according to mode
							if(mode == "advanced") {
								// advanced configuration: show all entries
								thisClass.content += thisClass.advancedOpt(cat, opt, obj, value);
							}
							else {
								// simple configuration: hide options that are
								// not simple
								if(obj.simple) {
									// push simple option to array for sorting
									simple.push([obj.position, cat, opt, obj, value]);
								}
								else thisClass.content += thisClass.hiddenOpt(cat, opt, obj, value);
							}
						}
					}
					if(mode != "advanced") {
						// sort simple configuration
						simple.sort(thisClass.sortByPos);
						
						// show simple configuration
						for(var i = 0; i < simple.length; i++) {
							thisClass.content += thisClass.simpleOpt(simple[i][1], simple[i][2], simple[i][3], simple[i][4]);
						}
					}
					
					// end category
					thisClass.content += thisClass.endCat(cat);
				}
				
				// render configuration
				endTime = +new Date();
				console.log("Config(): Content created after " + msToStr(endTime - startTime) + ".");
				thisClass.renderToView();
			})
			.fail(function(jqXHR, textStatus, errorThrown) {
				alert('Could not load configuration data structure.\n\n' + textStatus);
			});
		}
	}
	
	// sort values according to position
	sortByPos(a, b) {
		return ((a[0] < b[0]) ? -1 : ((a[0] > b[0]) ? 1 : 0));
	}
	
	// add advanced option
	advancedOpt(cat, name, obj, value) {
		var result = "";
		result += this.startOpt(cat, obj.id, name, obj.type, value, false);
		result += this.label(name + ":");
		result += this.startValue(obj.description);
		if(obj.type == "bool") result += this.check(cat, obj.id, value);
		else if(obj.type == "string") result += this.text(cat, obj.id, value, obj.default);
		else if(obj.type == "number") result += this.number(cat, obj.id, value, obj.default, obj.min, obj.max, obj.step);
		else if(obj.type == "date") result += this.date(cat, obj.id, value, obj.default, obj.min, obj.max);
		else if(obj.type == "array") {
			result += this.array(cat, obj.id, obj["item-type"], value, obj.min, obj.max, obj.step, this.getFilter(obj), obj["enum-values"],
					obj.default);
		}
		else if(obj.type == "query") result += this.query(cat, obj.id, value, this.getFilter(obj));
		else if(obj.type == "enum") result += this.valuelist(cat, obj.id, value, obj["enum-values"]);
		result += this.endValue();
		result += this.endOpt();
		return result;
	}
	
	// add hidden option
	hiddenOpt(cat, name, obj, value) {
		var result = "";
		result += this.startOpt(cat, obj.id, name, obj.type, value, true);
		result += this.endOpt();
		return result;
	}
	
	// add simple option
	simpleOpt(cat, name, obj, value) {
		var result = "";
		result += this.startOpt(cat, obj.id, name, obj.type, value, false);
		if(obj.type == "bool") result += this.label("", "");
		else result += this.label(obj.label);
		result += this.startValue(obj.description);
		if(obj.type == "bool") {
			result += this.check(cat, obj.id, value);
			result += this.label(obj.label);
		}
		else if(obj.type == "string") result += this.text(cat, obj.id, value, obj.default);
		else if(obj.type == "number") result += this.number(cat, obj.id, value, obj.default, obj.min, obj.max, obj.step);
		else if(obj.type == "date") result += this.date(cat, obj.id, value, obj.default, obj.min, obj.max);
		else if(obj.type == "array")
			result += this.array(cat, obj.id, obj["item-type"], value, obj.min, obj.max, obj.step, this.getFilter(obj), obj["enum-values"],
					obj.default);
		else if(obj.type == "query") result += this.query(cat, obj.id, value, this.getFilter(obj));
		else if(obj.type == "enum") result += this.valuelist(cat, obj.id, value, obj["enum-values"]);
		result += this.endValue();
		result += this.endOpt();
		return result;
	}
	
	/*
	 * RENDER FUNCTIONS
	 */
	
	// start category
	startCat(id, name, label, open) {
		var state, arrow;
		
		if(open) {
			state = "open";
			arrow = "&dArr;";
		}
		else {
			state = "closed";
			arrow = "&rArr;";
		}
		
		var result = "";
		result += "<!-- " + name + " BEGIN -->\n";
		result += "<div class=\"segment-head-" + state + "\" data-block=\"" + id + "\" unselectable=\"on\">\n";
		result += "<span class=\"segment-arrow\" data-block=\"" + id + "\">" + arrow + "</span> " + label + "\n";
		result += "</div>\n";
		result += "<div class=\"segment-body-" + state + "\" data-block=\"" + id + "\">\n";
		return result;
	}

	// end category
	endCat(name) {
		var result = "";
		result += "</div>\n";
		result += "<!-- " + name + " END -->\n"
		return result;
	}
	
	// start option
	startOpt(cat, id, name, type, value, hidden) {
		var result = "";
		result += "<div class=\"opt-block";
		if(hidden) result += " opt-hidden";
		result += "\" data-name=\"" + name + "\" data-id=\"" + id + "\" data-cat=\"" + cat + "\" data-type=\"" + type + "\"";
		result += " data-value=\'" + JSON.stringify(value) + "\'>\n";
		return result;
	}
	
	// end option
	endOpt() {
		var result = "";
		result += "</div>";
		return result;
	}
	
	// start value
	startValue(ttip)  {
		var result = "";
		result += "<div class=\"opt-value\" data-tippy-content=\"" + ttip + "\">\n";
		return result;
	}
	
	// end value
	endValue() {
		var result = "";
		result += "</div>\n";
		return result;
	}
	
	// show label
	label(name) {
		var result = "";
		result += "<div class=\"opt-label\">" + name + "</div>\n";
		return result;
	}
	
	// show checkbox
	check(cat, id, value, array = false) {
		var result = "";
		result += "<input type=\"checkbox\" class=\"opt\" ";
		
		if(!array) result += "id=\"opt-" + cat + "-" + id + "\" ";
		
		if(value) result += "checked ";
		
		result += "/> ";
		return result;
	}
	
	// show text input field
	text(cat, id, value, def, array = false) {
		var result = "";
		result += "<input type=\"text\" class=\"opt";
		
		if(array) result += " opt-array";
		
		result += "\" ";
		
		if(!array) result += "id=\"opt-" + cat + "-" + id + "\"";
		
		result += "placeholder=\"";
		
		if(!def.length) result += "[empty]";
		else result += def;
		
		result += "\"";
		
		if(def != value) result += " value=\"" + value + "\"";
		
		result += " />";
		return result;
	}
	
	// show number input field
	number(cat, id, value, def, min, max, step, array = false) {
		var result = "";
		result += "<input type=\"number\" class=\"opt";
		
		if(array) result += " opt-array";

		result += "\" ";
		
		if(!array) result += "id=\"opt-" + cat + "-" + id + "\" ";
		
		result += "placeholder=\"" + def + "\"";
		
		if(def != value) result += " value=\"" + value + "\"";
		
		if(typeof min !== "undefined") result += " min=\"" + min + "\"";
		if(typeof max !== "undefined") result += " max=\"" + max + "\"";
		if(step) result += " step=\"" + step + "\"";
		
		result += " />";
		return result;
	}
	
	// show date input field
	date(cat, id, value, def, min, max, array = false) {
		var result = "";
		result += "<input type=\"date\" class=\"opt";
		
		if(array) result += " opt-array";
		
		result += "\" ";
		
		if(!array) result += "id=\"opt-" + cat + "-" + id + "\" ";
		
		if(typeof min !== "undefined") result += " min=\"" + min + "\"";
		if(typeof max !== "undefined") result += " max=\"" + max + "\"";
		
		result += " />";
		return result;
	}
	
	// show query
	query(cat, id, value, filter, array = false) {		
		var result = "";
		result += "<select class=\"opt";
		
		if(array) result += " opt-array";
		
		result += "\"";
		
		if(!array) result += " id=\"opt-" + cat + "-" + id + "\"";
		
		result += ">\n";
		
		// no query option
		result += "<option value=\"0\"";
		if(!value) result += " selected";
		result += ">[none]</option>\n";
		
		// get query options from database
		for(var i = 0; i < db_queries.length; i++) {
			// check filter
			if(this.checkFilter(db_queries[i], filter)) {
				// show query as option
				result += "<option value=\"" + db_queries[i].id + "\"";
				if(value == db_queries[i].id) result += " selected";
				result += ">" + db_queries[i].name + "</option>\n";
			}
		}
		
		result += "</select>";
		return result;
	}
	
	// show list of values
	valuelist(cat, id, value, enumvalues, array = false) {
		var result = "";
		result += "<select class=\"opt";
		
		if(array) result += " opt-array";
		
		result += "\"";
		
		if(!array) result += " id=\"opt-" + cat + "-" + id + "\"";
		
		result += ">\n";
		
		// add enum values
		for(var i = 0; i < enumvalues.length; i++) {
			result += "<option value=\"" + i + "\"";
			if(value == i) result += " selected";
			result += ">" + enumvalues[i] + "</option>\n";
		}
		
		result += "</select>";
		return result;
	}
	
	// show array
	array(cat, id, type, value, min, max, step, filter, enumvalues, def) {
		var counter = 1;
		var isdef = false;
		var result = "";
		
		if(!Array.isArray(def))
			console.log("Error: Invalid default value for " + cat + ".#" + id + " (should be an array of " + type + ", but is \'"
					+ def + "\').");
		else isdef = def.equals(value);
		
		result += "<div class=\"opt-array\" id=\"opt-" + cat + "-" + id + "\" data-cat=\"" + cat + "\" data-id=\"" + id;
		result += "\" data-count=\"" + value.length + "\" data-type=\"" + type + "\"";
		if(typeof min !== "undefined") result += " data-min=\"" + min + "\"";
		if(typeof max !== "undefined") result += " data-max=\"" + max + "\"";
		if(typeof step !== "undefined") result += " data-step=\"" + step + "\"";
		if(typeof filter !== "undefined") result += " data-filter=\"" + filter + "\"";
		if(typeof enumvalues !== "undefined") result += " data-enum-values=\'" + JSON.stringify(enumvalues) + "\'";
		result += ">\n";
		if(value.length) {
			for(var i = 0; i < value.length; i++) {
				result += this.arrayitem(counter, cat, id, type, value[i], min, max, step, filter, enumvalues, isdef);
				counter++;
			}
		}
		else result += this.emptyarray(cat, id, type);
		result += "</div>\n";
		return result;
	}
	
	// show empty array
	emptyarray(cat, id, type) {
		var result = "";
		result += "<div class=\"opt-array-item\" data-item=\"0\">";
		result += "<div class=\"opt-empty\">[empty]</div>\n";
		result += "<input type=\"button\" class=\"opt-array-btn opt-array-add\" data-cat=\"" + cat + "\" data-id=\"" + id;
		result += "\" value=\"+\" />";
		result += "</div>\n";
		return result;
	}
	
	// show array item
	arrayitem(counter, cat, id, type, value, min, max, step, filter, enumvalues, isdef) {
		var result = "";
		result += "<div class=\"opt-array-item\" data-item=\"" + counter + "\">";
		result += "<span class=\"opt-array-count\">[" + counter + "] </span>";
		
		if(type == "bool") result += this.check(cat, id, value, true);
		else if(type == "string") {
			if(isdef) result += this.text(cat, id, value, value, true);
			else result += this.text(cat, id, value, "", true);
		}
		else if(type == "number") {
			if(isdef) result += this.number(cat, id, value, value, min, max, step, true);
			else result += this.number(cat, id, value, "", min, max, step, true);
		}
		else if(type == "date") {
			if(isdef) result += this.date(cat, id, value, value, min, max, true);
			else result += this.date(cat, id, value, "", min, max, true);
		}
		else if(type == "query") result += this.query(cat, id, value, filter, true);
		else if(type == "enum") result += this.valuelist(cat, id, value, enumvalues, true);
	
		result += "<input type=\"button\" class=\"opt-array-btn opt-array-del\" data-cat=\"" + cat + "\" data-id=\"" + id;
		result += "\" data-item=\"" + counter + "\" value=\"-\" />";
		result += "<input type=\"button\" class=\"opt-array-btn opt-array-add\" data-cat=\"" + cat + "\" data-id=\"" + id;
		result += "\" value=\"+\" />";			
		
		result += "</div>\n";
		return result;
	}
	
	// render configuration to view
	renderToView() {
		var startTime = +new Date();
		this.view.html(this.content);
		
		// alternate rows
		var optvalues = document.getElementsByClassName("opt-block");
		var i = 0;
		for(i = 0; i < optvalues.length; i++) {
			if(!optvalues[i].classList.contains("opt-hidden") && (i + 1) % 2 == 0) optvalues[i].classList.add("opt-block-alternate");
		}
		
		// activate tooltips
		tippy(".opt-value", { delay : 0, duration: 0, arrow : true, placement: "left-start", size : "small" });
		console.log("Config::renderToView(): Rendering took " + msToStr(+new Date() - startTime) + ".");
	}
	
	/*
	 * EVENT FUNCTIONS
	 */
	
	// add element to array
	onAddElement(cat, id) {
		var array = $("div.opt-array[data-cat=\'" + cat + "\'][data-id=\'" + id + "\']");
		var count = array.data("count");
		array.find("div.opt-array-item[data-item=0]").remove();
		array.append(this.arrayitem(count + 1, cat, id, array.data("type"), "", array.data("min"), array.data("max"),
				array.data("step"), array.data("filter"), array.data("enum-values"), false));
		array.data("count", count + 1);
	}
	
	// remove element from array
	onDelElement(cat, id, item) {
		var array = $("div.opt-array[data-cat=\'" + cat + "\'][data-id=\'" + id + "\']");
		var count = array.data("count");
		var toRemove = null;
		
		// rename and renumber
		array.find("div.opt-array-item").each(function(i, obj) {			
  			var objn = parseInt($(obj).data("item"), 10);
  			if(item == objn) {
  				toRemove = obj;
  			}
  			else if(item < objn) {
				objn--;
  				$(obj).find("span.opt-array-count").text("[" + objn + "] ");
  				$(obj).find("input.opt-array-del").data("item", objn);
  				$(obj).data("item", objn);
  			}
		});
		
		// refresh item count
		array.data("count", count - 1);
		
		// check for empty array
		if(count == 1) array.append(this.emptyarray(array.data("cat"), array.data("id"), array.data("type")));
		
		// delete element
		toRemove.remove();
	}
	
	/*
	 * DATA FUNCTIONS
	 */
	
	// get value from current configuration
	getConfValue(cat, opt) {
		for(var i = 0; i < this.config_combined.length; i++) {
			if(this.config_combined[i].cat == cat && this.config_combined[i].name == opt)
				return this.config_combined[i].value;
		}
		console.log("Error: Could not find \'" + cat + "." + opt + "\' in current configuration.");
	}
	
	// update and get current configuration
	updateConf() {
		// update configuration: go through all categories and options in base
		for(var cat in this.config_base) {
			for(var opt in this.config_base[cat]) {
				let obj = this.config_base[cat][opt];
				
				// find element(s)
				var element = $("#opt-" + cat + "-" + obj.id);
				if($(element).length) {					
					// search for option in current configuration
					for(var i = 0; i < this.config_current.length; i++) {
						let cobj = this.config_current[i];
						if(cobj.cat == cat && cobj.name == opt) {
							// found option in current configuration: delete it
							this.config_current.splice(i, 1);
							break;
						}
					}
					
					// check type and retrieve value
					var nobj = { "cat": cat, "name": opt, "value": null };
					if(obj.type == "bool") nobj.value = $(element).is(":checked");
					else if(obj.type == "string") {
						// placeholder means default value
						if(!($(element).val().length) && $(element).attr("placeholder") !== undefined) nobj.value = obj.default;
						else nobj.value = $(element).val();
					}
					else if(obj.type == "number") {
						// placeholder means default value
						if(!($(element).val().length) && $(element).attr("placeholder") !== undefined) nobj.value = obj.default;
						else if($(element).val().length) nobj.value = parseInt($(element).val(), 10);
						else nobj.value = 0;
					}
					else if(obj.type == "date") nobj.value = $(element).val();
					else if(obj.type == "query") nobj.value = parseInt($(element).val(), 10);
					else if(obj.type == "enum") nobj.value = parseInt($(element).val(), 10);
					else if(obj.type == "array") {
						nobj.value = [];
						$(element).find(".opt").each(function() {
							// get item number from parent
							var index = $(this).parent(".opt-array-item").data("item");
							if(index > 0) {
								index--;
								
								// add value to array according to type
								if(obj["item-type"] == "bool") nobj.value[index] = $(this).is(":checked");
								else if(obj["item-type"] == "string") {
									// placeholder means default value (except
									// when default value does not exist)
									if(!($(this).val().length) && $(this).attr("placeholder") !== undefined) {
										if(index < obj.default.length) nobj.value[index] = obj.default[index];
									}
									else {
										nobj.value[index] = $(this).val();
									}
								}
								else if(obj["item-type"] == "number") {
									// placeholder means default value (except
									// when default value does not exist)
									if(!($(this).val().length) && $(this).attr("placeholder") !== undefined) {
										if(index < obj.default.length) nobj.value[index] = obj.default[index];
									}
									else if($(this).val().length) nobj.value[index] = parseInt($(this).val(), 10);
								}
								else if(obj["item-type"] == "date") nobj.value[index] = $(this).val();
								else if(obj["item-type"] == "query") {
									var id = parseInt($(this).val(), 10);
									if(id) nobj.value[index] = id; 
								}
								else if(obj["item-type"] == "enum") nobj.value[index] = parseInt($(this).val(), 10);
							}
						});
 					}
					
					// add new object to the current configuration
					if(nobj.value != null) this.config_current.push(nobj);
					else console.log("Error: Could not get value of '" + cat + "." + opt + "'. Invalid type?");
				}
			}
		}
		
		// remove default values from configuration
		for(var i = 0; i < this.config_current.length; i++) {
			let opt = this.config_current[i];
			if(this.config_base[opt.cat][opt.name] === undefined) {
				if(this.config_current.splice(i, 1).length < 1)
					console.log("Error deleting " + opt.cat + "." + opt.name + " from current configuration.");
				else {
					console.log("Error: Invalid option \'" + opt.cat + "." + opt.name + "\' removed.");
					i--;
				}
				continue;
			}
			let def = this.config_base[opt.cat][opt.name].default;
			if(Array.isArray(opt.value)) {
				if(opt.value.equals(def)) {
					if(this.config_current.splice(i, 1).length < 1)
						console.log("Error deleting " + opt.cat + "." + opt.name + " from current configuration.");
					else i--;
				}
			}
			else {
				if(opt.value == def) {
					if(this.config_current.splice(i, 1).length < 1)
						console.log("Error deleting " + opt.cat + "." + opt.name + " from current configuration.");
					else i--;
				}
			}
		}
		
		// return JSON
		return this.config_current;
	}
	
	// check for changes
	isConfChanged() {
		return JSON.stringify(this.updateConf(), Object.keys(flattenObject(this.updateConf())).sort())
			!= JSON.stringify(this.config_db, Object.keys(flattenObject(this.config_db)).sort())
	}
	
	/*
	 * HELPER FUNCTIONS
	 */
	
	// get filter string for queries
	getFilter(obj) {
		var filter = "";
		if(obj.type != "query" && obj["item-type"] != "query") return undefined;
		if(typeof obj["query-results"] !== "undefined" && obj["query-results"].length) {
			if(obj["query-results"].includes("bool")) filter += "X";
			else filter += "0";
			if(obj["query-results"].includes("single")) filter += "X";
			else filter += "0";
			if(obj["query-results"].includes("multi")) filter += "X";
			else filter += "0";
		}
		else filter += "XXX"
		if(typeof obj["query-types"] !== "undefined" && obj["query-types"].length) {
			if(obj["query-types"].includes("regex")) filter += "X";
			else filter += "0";
			if(obj["query-types"].includes("jquery")) filter += "X";
			else filter += "0";
		}
		else filter += "XX";
		return filter;
	}
	
	// check query against filter
	checkFilter(obj, filter) {
		var result = false;
		if(filter[0] == "X" && obj.resultbool) result = true;
		if(filter[1] == "X" && obj.resultsingle) result = true;
		if(filter[2] == "X" && obj.resultmulti) result = true;
		if(!result) return false;
		if(filter[3] == "X" && obj.type == "regex") return true;
		if(filter[4] == "X" && obj.type == "xpath") return true;
		return false;
	}
}