
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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *  
 * ---
 * 
 * config.js
 * 
 * Configuration class for the PHP/JavaScript crawlserv++ frontend.
 *  
 */

// @requires helpers.js !

class Config {
	
	/*
	 * GENERAL FUNCTIONS
	 */
	
	// constructor: create or load configuration
	constructor(module, json, mode, filter = null, removal = null, callback_when_finished = null) {
		console.log("Config::constructor(): Loading configuration...");
		
		// link class to view
		this.view = $("#config-view");
		
		if(!this.view)
			throw new Error(
					"Config::constructor(): Content view not found."
			);
		
		// set empty content
		this.content = "";
		
		// save start time
		var startTime = +new Date();
		
		// save class pointer
		let thisClass = this;
		
		// parse configuration data from JSON file for module
		console.log("> json/" + module + ".json");
		
		$.getJSON("json/" + module + ".json", function(data) {
			// get algo data
			var deferreds = [];
			var categories = [];
			var results = [];
			var includes = [];
			
			// get data for algorithms from external JSONs
			if(typeof algo !== 'undefined') {
				$.each(algo.config_cats, function(index, category) {
					if(category != "general") {
						categories.push(category);
						
						console.log("> json/algos/" + category + ".json");
						
						deferreds.push($.getJSON("json/algos/" + category + ".json", function(data) {
							results.push(data);
						}));
					}
				});
			}
			
			// get included data from external JSONs
			if(data.hasOwnProperty("#include")) {
				if($.isArray(data["#include"])) {
					$.each(data["#include"], function(index, include) {
						console.log("> json/" + include);
						
						deferreds.push($.getJSON("json/" + include, function(data) {
							includes.push(data);
						}));
					});
				}
				else {
					console.log("> json/" + data["#include"]);
					
					deferreds.push($.getJSON("json/" + data["#include"], function(data) {
						includes.push(data);
					}));
				}
				
				delete data["#include"];
			}
			
			$.when.apply($, deferreds).then(function() {
				// include algo data				
				$.each(results, function(index, algoData) {
					data[categories[index]] = algoData;
				});
				
				// merge with included JSON data
				$.each(includes, function(index, include) {
					$.extend(data, include);
				})
				
				thisClass.config_base = data;
								
				// generate IDs for categories and entries
				var catCounter = 0;
				
				for(var cat in thisClass.config_base) {
					catCounter++;
					
					thisClass.config_base[cat].id = catCounter;
					
					var idCounter = 0;
					
					for(var opt in thisClass.config_base[cat]) {
						// ignore category properties
						if(thisClass.config_base[cat].hasOwnProperty(opt)
								&& opt != "id"
								&& opt != "open"
								&& opt != "label") {
							idCounter++;
							
							thisClass.config_base[cat][opt].id = idCounter;
							
							// check whether default value exists
							if(typeof thisClass.config_base[cat][opt].default === "undefined") {
								var warningStr = "WARNING: Default value for \"" + cat + "." + opt + "\" is missing, ";
								
								if(thisClass.config_base[cat][opt].type == "array") {
									thisClass.config_base[cat][opt].default = [];
									warningStr += "set to empty array.";
								}
								else if(thisClass.config_base[cat][opt].type == "string") {
									thisClass.config_base[cat][opt].default = "";
									warningStr += "set to empty string.";
								}
								else {
									thisClass.config_base[cat][opt].default = 0;
									warningStr += "set to zero.";
								}
								
								console.log(warningStr);
							}
						}
					}
				}
			
				// calculate loading time
				var endTime = +new Date();
				
				console.log(
						"Config::constructor(): Configuration data loaded" +
						" after " + msToStr(endTime - startTime) + "."
				);
				
				startTime = endTime;
				
				// parse configuration from database
				thisClass.config_db = db_config;
				
				// parse current configuration
				thisClass.config_current = $.parseJSON(json);
				
				// remove categories of old algorithm
				if(removal !== null)
					thisClass.purgeCategories(removal);
				
				// combine configurations
				thisClass.config_combined = [];
				
				for(var cat in thisClass.config_base) {
					for(var opt in thisClass.config_base[cat]) {
						// ignore category properties
						if(thisClass.config_base[cat].hasOwnProperty(opt)
								&& opt != "id"
								&& opt != "open"
								&& opt != "label") {
							var optobj = {};
							
							optobj.cat = cat;
							optobj.name = opt;
							optobj.value = thisClass.config_base[cat][opt].default;
							
							// overwrite value from database
							for(var i = 0; i < thisClass.config_db.length; i++)
								if(thisClass.config_db[i].cat == cat && thisClass.config_db[i].name == opt)
									optobj.value = thisClass.config_db[i].value;
							
							// overwrite value from current configuration
							for(var i = 0; i < thisClass.config_current.length; i++)
								if(thisClass.config_current[i].cat == cat && thisClass.config_current[i].name == opt)
									optobj.value = thisClass.config_current[i].value;
							
							thisClass.config_combined.push(optobj);
						}
					}
				}				
				
				// create configuration content: go through all categories
				for(var cat in thisClass.config_base) {
					var simple = [];
					
					// check whether filter is enabled
					if(filter != null) {
						var found = false;
						
						// search for category in filter
						for(var whitelisted of filter) {
							if(whitelisted == cat) {
								found = true;
								
								break;
							}
						}
						if(!found)
							// category not in filter: do not render it
							continue;
					}
					
					// start category
					let cobj = thisClass.config_base[cat];
					
					thisClass.content += thisClass.startCat(cobj.id, cat, cobj.label, cobj.open);
					
					// go through all options in category
					for(var opt in thisClass.config_base[cat]) {
						// ignore category (and internal JavaScript) properties
						if(cobj.hasOwnProperty(opt)
								&& opt != "id"
								&& opt != "open"
								&& opt != "label") {
							// set current object
							let obj = cobj[opt];
							
							// get value
							var value = thisClass.getConfValue(cat, opt);
							
							// show option according to mode
							if(mode == "advanced")
								// advanced configuration: show all entries
								thisClass.content += thisClass.advancedOpt(cat, opt, obj, value);
							else {
								// simple configuration: hide options that are not simple
								if(obj.simple)
									// push simple option to array for sorting
									simple.push([obj.position, cat, opt, obj, value]);
								else
									thisClass.content += thisClass.hiddenOpt(
											cat, opt, obj, value
									);
							}
						}
					}
					if(mode != "advanced") {
						// sort simple configuration
						simple.sort(thisClass.sortByPos);
						
						// show simple configuration
						for(var i = 0; i < simple.length; i++)
							thisClass.content += thisClass.simpleOpt(
									simple[i][1], simple[i][2], simple[i][3], simple[i][4]
							);
					}
					
					// end category
					thisClass.content += thisClass.endCat(cat);
				}
				
				// render configuration
				endTime = +new Date();
				
				console.log(
						"Config::constructor(): Content created after" +
						" " + msToStr(endTime - startTime) + "."
				);
				
				thisClass.renderToView();
				
				// success!
				if(callback_when_finished != null)
					callback_when_finished();
			})
			.fail(function(jqXHR, textStatus, errorThrown) {
				handleJsonError(
						"Config::constructor(): Could not load configuration data.",
						jqXHR,
						textStatus,
						errorThrown,
				);
			});
		})
		.fail(function(jqXHR, textStatus, errorThrown) {
			handleJsonError(
					"Config::constructor(): Could not load configuration data.",
					jqXHR,
					textStatus,
					errorThrown
			);
		});
	}
	
	// sort values according to position
	sortByPos(a, b) {
		return ((a[0] < b[0])
				? -1 : ((a[0] > b[0])
				? 1 : 0));
	}
	
	// add advanced option
	advancedOpt(cat, name, obj, value) {
		var result = "";
		
		result += this.startOpt(cat, obj.id, name, obj.type, value, false);
		result += this.label(name + ":");
		result += this.startValue(obj.description);
		
		if(obj.type == "bool")
			result += this.check(
					cat,
					obj.id,
					value
			);
		else if(obj.type == "string")
			result += this.text(
					cat,
					obj.id,
					value,
					obj.default
			);
		else if(obj.type == "number")
			result += this.number(
					cat,
					obj.id,
					value,
					obj.default,
					obj.min,
					obj.max,
					obj.step
			);
		else if(obj.type == "date")
			result += this.date(
					cat,
					obj.id,
					value,
					obj.default,
					obj.min,
					obj.max
			);
		else if(obj.type == "array") {
			result += this.array(
					cat,
					obj.id,
					obj["item-type"],
					value,
					obj.min,
					obj.max,
					obj.step,
					this.getFilter(obj),
					obj["enum-values"],
					obj.default
			);
		}
		else if(obj.type == "locale")
			result += this.locale(
					cat,
					obj.id,
					value,
			);
		else if(obj.type == "query")
			result += this.query(
					cat,
					obj.id,
					value,
					this.getFilter(obj)
			);
		else if(obj.type == "enum")
			result += this.valuelist(
					cat,
					obj.id,
					value,
					obj["enum-values"]
			);
		
		result += this.endValue();
		result += this.endOpt();
		
		return result;
	}
	
	// add hidden option
	hiddenOpt(cat, name, obj, value) {
		var result = "";
		
		result += this.startOpt(
				cat,
				obj.id,
				name,
				obj.type,
				value,
				true
		);
		result += this.endOpt();
		
		return result;
	}
	
	// add simple option
	simpleOpt(cat, name, obj, value) {
		var result = "";
		
		result += this.startOpt(
				cat,
				obj.id,
				name,
				obj.type,
				value,
				false
		);
		
		if(obj.type == "bool")
			result += this.label(
					""
			);
		else
			result += this.label(
					obj.label
			);
		
		result += this.startValue(
				obj.description
		);
		
		if(obj.type == "bool") {
			result += this.check(
					cat,
					obj.id,
					value
			);
			result += this.label(
					obj.label
			);
		}
		else if(obj.type == "string")
			result += this.text(
					cat,
					obj.id,
					value,
					obj.default
			);
		else if(obj.type == "number")
			result += this.number(
					cat,
					obj.id,
					value,
					obj.default,
					obj.min,
					obj.max,
					obj.step
			);
		else if(obj.type == "date")
			result += this.date(
					cat,
					obj.id,
					value,
					obj.default,
					obj.min,
					obj.max
			);
		else if(obj.type == "array")
			result += this.array(
					cat,
					obj.id,
					obj["item-type"],
					value,
					obj.min,
					obj.max,
					obj.step,
					this.getFilter(obj),
					obj["enum-values"],
					obj.default
			);
		else if(obj.type == "locale")
			result += this.locale(
					cat,
					obj.id,
					value
			);			
		else if(obj.type == "query")
			result += this.query(
					cat,
					obj.id,
					value,
					this.getFilter(obj)
			);
		else if(obj.type == "enum")
			result += this.valuelist(
					cat,
					obj.id,
					value,
					obj["enum-values"]
			);
		
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
		
		return	"<!-- " + name + " BEGIN -->\n \
				 <div class=\"segment-head-" + state + "\" data-block=\"" + id + "\" unselectable=\"on\">\n \
				  <span class=\"segment-arrow\" data-block=\"" + id + "\">"
				   + arrow + "\
				  </span> " + label + "\n \
				 </div>\n \
				 <div class=\"segment-body-" + state + "\" data-block=\"" + id + "\">\n";
	}

	// end category
	endCat(name) {
		return	"</div>\n \
				 <!-- " + name + " END -->\n";
	}
	
	// start option
	startOpt(cat, id, name, type, value, hidden) {
		var result = "";
		
		result +=	"<div class=\"opt-block";
		
		if(hidden)
			result += " opt-hidden";
		
		result +=	"\" data-name=\"" + name +
					"\" data-id=\"" + id +
					"\" data-cat=\"" + cat +
					"\" data-type=\"" + type +
					"\" data-value=\'" + JSON.stringify(value) +
					"\"'>\n";
					
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
		
		if(!array)
			result += "id=\"opt-" + cat + "-" + id + "\" ";
		if(value)
			result += "checked ";
		
		result += "/> ";
		
		return result;
	}
	
	// show text input field
	text(cat, id, value, def, array = false) {
		var result = "";
		
		if(value == null)
			value = "";
		
		result += "<input type=\"text\" class=\"opt";
		
		if(array) result += " opt-array";
		
		result += "\" ";
		
		if(!array) result += "id=\"opt-" + cat + "-" + id + "\"";
		
		result += "placeholder=\"";
		
		if(!def.length)
			result += "[empty]";
		else
			result += def;
		
		result += "\"";
		
		if(def != value)
			result += " value=\"" + value + "\"";
		
		result += " />";
		
		return result;
	}
	
	// show number input field
	number(cat, id, value, def, min, max, step, array = false) {
		var result = "";
		
		result += "<input type=\"number\" class=\"opt";
		
		if(array)
			result += " opt-array";

		result += "\" ";
		
		if(!array)
			result += "id=\"opt-" + cat + "-" + id + "\" ";
		
		result += "placeholder=\"" + def + "\"";
		
		if(def != value)
			result += " value=\"" + value + "\"";
		
		if(typeof min !== "undefined")
			result += " min=\"" + min + "\"";
		if(typeof max !== "undefined")
			result += " max=\"" + max + "\"";
		
		if(step)
			result += " step=\"" + step + "\"";
		
		result += " />";
		
		return result;
	}
	
	// show date input field
	date(cat, id, value, def, min, max, array = false) {
		var result = "";
		
		result += "<input type=\"date\" class=\"opt";
		
		if(array)
			result += " opt-array";
		
		result += "\" ";
		
		if(!array)
			result += "id=\"opt-" + cat + "-" + id + "\" ";
		
		result += "placeholder=\"" + def + "\"";
		
		if(def != value)
			result += " value=\"" + value + "\"";
		
		if(typeof min !== "undefined")
			result += " min=\"" + min + "\"";
		if(typeof max !== "undefined")
			result += " max=\"" + max + "\"";
		
		result += " />";
		
		return result;
	}
	
	// show locale
	locale(cat, id, value, array = false) {		
		var result = "";
		
		result += "<select class=\"opt";
		
		if(array)
			result += " opt-array";
		
		result += "\"";
		
		if(!array)
			result += " id=\"opt-" + cat + "-" + id + "\"";
		
		result += ">\n";
		
		// no locale option
		result += "<option value=\"\"";
		
		if(!value)
			result += " selected";
		
		result += ">[none]</option>\n";
		
		// get locales from database
		for(var i = 0; i < db_locales.length; i++) {
			// show locale as option
			result += "<option value=\"" + db_locales[i] + "\"";
			
			if(value && value.toLowerCase() == db_locales[i].toLowerCase())
				result += " selected";
			
			result += ">" + db_locales[i] + "</option>\n";
		}
		
		return result;
	}
	
	// show query
	query(cat, id, value, filter, array = false) {		
		var result = "";
		
		result += "<select class=\"opt";
		
		if(array)
			result += " opt-array";
		
		result += "\"";
		
		if(!array)
			result += " id=\"opt-" + cat + "-" + id + "\"";
		
		result += ">\n";
		
		// no query option
		result += "<option value=\"0\"";
		
		if(!value)
			result += " selected";
		
		result += ">[none]</option>\n";
		
		// get query options from database
		for(var i = 0; i < db_queries.length; i++) {
			// check filter
			if(this.checkFilter(db_queries[i], filter)) {
				// show query as option
				result += "<option value=\"" + db_queries[i].id + "\"";
				
				if(value == db_queries[i].id)
					result += " selected";
				
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
		
		if(array)
			result += " opt-array";
		
		result += "\"";
		
		if(!array)
			result += " id=\"opt-" + cat + "-" + id + "\"";
		
		result += ">\n";
		
		// add enum values
		for(var i = 0; i < enumvalues.length; i++) {
			result += "<option value=\"" + i + "\"";
			
			if(value == i)
				result += " selected";
			
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
		
		if(!$.isArray(def))
			throw new Error(
					"config::array(): Invalid default value for '"
					+ cat
					+ ".#"
					+ id
					+ "' (should be an array of "
					+ type
					+ ", but is \'"
					+ def
					+ "\')."
			);
		
		isdef = def.equals(value);
		
		result += "<div class=\"opt-array\" \
					id=\"opt-" + cat + "-" + id + "\" \
					data-cat=\"" + cat + "\" \
					data-id=\"" + id + "\" \
					data-count=\"" + value.length + "\" \
					data-type=\"" + type + "\"";
		
		if(typeof min !== "undefined")
			result += " data-min=\"" + min + "\"";
		if(typeof max !== "undefined")
			result += " data-max=\"" + max + "\"";
		if(typeof step !== "undefined")
			result += " data-step=\"" + step + "\"";
		if(typeof filter !== "undefined")
			result += " data-filter=\"" + filter + "\"";
		if(typeof enumvalues !== "undefined")
			result += " data-enum-values=\'" + JSON.stringify(enumvalues) + "\'";
		
		result += ">\n";
		
		if(value.length) {
			for(var i = 0; i < value.length; i++) {
				result += this.arrayitem(
						counter,
						cat,
						id,
						type,
						value[i],
						min,
						max,
						step,
						filter,
						enumvalues,
						isdef);
				
				counter++;
			}
		}
		else result += this.emptyarray(cat, id, type);
		result += "</div>\n";
		return result;
	}
	
	// show empty array
	emptyarray(cat, id, type) {
		return "<div class=\"opt-array-item\" data-item=\"0\"> \
				<div class=\"opt-empty\">[empty]</div>\n \
				<input type=\"button\" \
				 class=\"opt-array-btn opt-array-add\" \
				 data-cat=\"" + cat + "\" \
				 data-id=\"" + id + "\" \" \
				 value=\"+\" /> \
				 </div>\n";
	}
	
	// show array item
	arrayitem(counter, cat, id, type, value, min, max, step, filter, enumvalues, isdef) {
		var result = "<div class=\"opt-array-item\" data-item=\"" + counter + "\">\
					  <span class=\"opt-array-count\">[" + counter + "] </span>";
		
		if(type == "bool") {
			result += this.check(cat, id, value, true);
			result += this.label("");
		}
		else if(type == "string") {
			if(isdef)
				result += this.text(cat, id, value, value, true);
			else
				result += this.text(cat, id, value, "", true);
		}
		else if(type == "number") {
			if(isdef)
				result += this.number(cat, id, value, value, min, max, step, true);
			else
				result += this.number(cat, id, value, "", min, max, step, true);
		}
		else if(type == "date") {
			if(isdef)
				result += this.date(cat, id, value, value, min, max, true);
			else
				result += this.date(cat, id, value, "", min, max, true);
		}
		else if(type == "locale")
			result += this.locale(cat, id, value, true);
		else if(type == "query")
			result += this.query(cat, id, value, filter, true);
		else if(type == "enum")
			result += this.valuelist(cat, id, value, enumvalues, true);
	
		result += "<input type=\"button\" \
					class=\"opt-array-btn opt-array-del\" \
					data-cat=\"" + cat + "\" \
					data-id=\"" + id + "\" \
					data-item=\"" + counter + "\" \
					value=\"-\" /><input type=\"button\" \
					class=\"opt-array-btn opt-array-add\" \
					data-cat=\"" + cat + "\" \
					data-id=\"" + id + "\" \
					value=\"+\" /></div>\n";
		
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
			if(!optvalues[i].classList.contains("opt-hidden")
					&& (i + 1) % 2 == 0)
				optvalues[i].classList.add("opt-block-alternate");
		}
		
		// activate tooltips
		tippy(
				".opt-value",
				{ delay : 0, duration: 0, arrow : true, placement: "left-start", size : "small" }
		);
		
		console.log(
				"Config::renderToView(): Rendering took " + msToStr(+new Date() - startTime) + "."
		);
	}
	
	/*
	 * EVENT FUNCTIONS
	 */
	
	// add element to array
	onAddElement(cat, id) {
		var array = $("div.opt-array[data-cat=\'" + cat + "\'][data-id=\'" + id + "\']");
		var count = array.data("count");
		
		array.find("div.opt-array-item[data-item=0]").remove();
		array.append(this.arrayitem(
				count + 1, cat, id, array.data("type"), "", array.data("min"), array.data("max"),
				array.data("step"), array.data("filter"), array.data("enum-values"), false
		));
		array.data("count", count + 1);
		array.children("div").last().children(
				"input[type=text], input[type=number], input[type=date]"
		).focus();
	}
	
	// remove element from array
	onDelElement(cat, id, item) {
		var array = $("div.opt-array[data-cat=\'" + cat + "\'][data-id=\'" + id + "\']");
		var count = array.data("count");
		var toRemove = null;
		
		// rename and renumber
		array.find("div.opt-array-item").each(function(i, obj) {			
  			var objn = parseInt($(obj).data("item"), 10);
  			if(item == objn)
  				toRemove = obj;
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
		if(count == 1)
			array.append(this.emptyarray(
					array.data("cat"), array.data("id"), array.data("type")
			));
		
		// delete element
		toRemove.remove();
	}
	
	/*
	 * DATA FUNCTIONS
	 */
	
	// set algorithm
	setAlgo(algo) {
		this.id = algo;
	}
	
	// get value from current configuration
	getConfValue(cat, opt) {
		for(var i = 0; i < this.config_combined.length; i++) {
			if(this.config_combined[i].cat == cat
					&& this.config_combined[i].name == opt)
				return this.config_combined[i].value;
		}
		throw new Error(
				"config::getConfValue(): Could not find \'"
				+ cat
				+ "."
				+ opt
				+ "\' in current configuration."
		);
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
					
					if(obj.type == "bool")
						nobj.value = $(element).is(":checked");
					else if(obj.type == "string") {
						// placeholder means default value
						if(!($(element).val().length)
								&& $(element).attr("placeholder") !== undefined)
							nobj.value = obj.default;
						else
							nobj.value = $(element).val();
					}
					else if(obj.type == "number") {
						// placeholder means default value
						if(!($(element).val().length)
								&& $(element).attr("placeholder") !== undefined)
							nobj.value = obj.default;
						else if($(element).val().length)
							nobj.value = parseInt($(element).val(), 10);
						else
							nobj.value = 0;
					}
					else if(obj.type == "date")
						nobj.value = $(element).val();
					else if(obj.type == "locale")
						nobj.value = $(element).val();
					else if(obj.type == "query")
						nobj.value = parseInt($(element).val(), 10);
					else if(obj.type == "enum")
						nobj.value = parseInt($(element).val(), 10);
					else if(obj.type == "array") {
						nobj.value = [];
						
						$(element).find(".opt").each(function() {
							// get item number from parent
							var index = $(this).parent(".opt-array-item").data("item");
							
							if(index > 0) {
								index--;
								
								// add value to array according to type
								if(obj["item-type"] == "bool")
									nobj.value[index] = $(this).is(":checked");
								else if(obj["item-type"] == "string") {
									// placeholder means default value (except
									// when default value does not exist)
									if(!($(this).val().length)
											&& $(this).attr("placeholder") !== undefined) {
										if(index < obj.default.length)
											nobj.value[index] = obj.default[index];
									}
									else {
										nobj.value[index] = $(this).val();
									}
								}
								else if(obj["item-type"] == "number") {
									// placeholder means default value (except
									// when default value does not exist)
									if(!($(this).val().length)
											&& $(this).attr("placeholder") !== undefined) {
										if(index < obj.default.length)
											nobj.value[index] = obj.default[index];
									}
									else if($(this).val().length)
										nobj.value[index] = parseInt($(this).val(), 10);
								}
								else if(obj["item-type"] == "date")
									nobj.value[index] = $(this).val();
								else if(obj["item-type"] == "locale")
									nobj.value[index] = $(this).val();
								else if(obj["item-type"] == "query") {
									var id = parseInt($(this).val(), 10);
									if(id) nobj.value[index] = id; 
								}
								else if(obj["item-type"] == "enum")
									nobj.value[index] = parseInt($(this).val(), 10);
							}
						});
 					}
					
					// add new object to the current configuration
					if(nobj.value == null) throw new Error(
							"Config::updateConf(): Could not get value of '"
							+ cat
							+ "."
							+ opt
							+ "' (of type '"
							+ obj.type
							+ "')."
					);
					else
						this.config_current.push(nobj); 
				}
			}
		}
		
		// remove default values from configuration
		for(var i = 0; i < this.config_current.length; i++) {
			let opt = this.config_current[i];
			
			if(opt.name == "_algo")
				continue;
			else if(this.config_base[opt.cat] === undefined) {
				if(this.config_current.splice(i, 1).length < 1)
					console.log(
							"Config::updateConf() WARNING:" +
							" Could not delete " + opt.cat + "." + opt.name +
							" from current configuration."
					);
				else
					i--;
				
				continue;
			}
			if(this.config_base[opt.cat][opt.name] === undefined) {
				if(this.config_current.splice(i, 1).length < 1)
					console.log(
							"Config::updateConf() WARNING:" +
							" Could not delete " + opt.cat + "." + opt.name +
							" from current configuration."
					);
				else {
					console.log(
							"Config::updateConf()" +
							" WARNING: Invalid option \'" + opt.cat + "." + opt.name + "\'" +
							" removed."
					);
					i--;
				}
				
				continue;
			}
			
			let def = this.config_base[opt.cat][opt.name].default;
			
			if($.isArray(opt.value)) {
				if(opt.value.equals(def)) {
					if(this.config_current.splice(i, 1).length < 1)
						console.log(
								"Config::updateConf()" +
								" WARNING: Could not delete " + opt.cat + "." + opt.name +
								" from current configuration."
						);
					else
						i--;
				}
			}
			else {
				if(opt.value == def) {
					if(this.config_current.splice(i, 1).length < 1)
						console.log(
								"Config::updateConf()" +
								" WARNING: Could not delete " + opt.cat + "." + opt.name +
								" from current configuration."
						);
					else
						i--;
				}
			}
		}
		
		// add or update algorithm if necessary
		if(typeof algo !== "undefined" && algo.id != null) {
			var found = false;
			
			for(var key in this.config_current) {
				if(this.config_current[key].name == "_algo") {
					this.config_current[key].value = algo.id;
					
					found = true;
					
					break;
				}
			}
			if(!found)
				this.config_current.unshift({ 'name': '_algo', 'value': algo.id });
		}
		
		// return JSON
		return this.config_current;
	}
	
	// check for changes
	isConfChanged() {
		var newConfig = this.updateConf();
		var oldConfig = this.config_db;
		
		// compare configurations
		return !areConfigsEqual(newConfig, oldConfig);
	}
	
	// check for the existence of algorithm-specific settings
	hasAlgoSettings() {
		var found = false;
		
		if(algo) {
			let thisClass = this;
			
			$.each(algo.config_cats, function(index, algoCat) {
				if(algoCat != "general") {
					$.each(thisClass.config_current, function(index, configEntry) {
						if(configEntry["name"] != "_algo" && configEntry["cat"] == algoCat) {
							found = true;
							
							return false; // break
						}
					});
				}
				
				if(found)
					return false; // break
			});
		}
		
		return found;
	}
	
	/*
	 * HELPER FUNCTIONS
	 */
	
	// get filter string for queries
	getFilter(obj) {
		var filter = "";
		
		if(
				obj.type != "query"
				&& obj["item-type"] != "query"
		)
			return undefined;
		
		if(
				typeof obj["query-results"] !== "undefined"
				&& obj["query-results"].length
		) {		
			if(obj["query-results"].includes("bool"))
				filter += "X";
			else
				filter += "0";
			
			if(obj["query-results"].includes("single"))
				filter += "X";
			else
				filter += "0";
			
			if(obj["query-results"].includes("multi"))
				filter += "X";
			else
				filter += "0";
			
			if(obj["query-results"].includes("subsets"))
				filter += "X";
			else
				filter += "0";
		}
		else
			filter += "XXXX";

		if(
				typeof obj["query-types"] !== "undefined"
				&& obj["query-types"].length
		) {
			
			if(obj["query-types"].includes("regex"))
				filter += "X";
			else
				filter += "0";
			
			if(obj["query-types"].includes("jquery"))
				filter += "X";
			else
				filter += "0";
			
			if(obj["query-types"].includes("jsonpointer"))
				filter += "X";
			else
				filter += "0";
			
			if(obj["query-types"].includes("jsonpath"))
				filter += "X";
			else
				filter += "0";
		}
		else
			filter += "XXXX";
		
		return filter;
	}
	
	// check query against filter
	checkFilter(obj, filter) {
		var result = false;
		
		if(filter[0] == "X" && obj.resultbool)
			result = true;
		
		if(filter[1] == "X" && obj.resultsingle)
			result = true;
		
		if(filter[2] == "X" && obj.resultmulti)
			result = true;
		
		if(filter[3] == "X" && obj.resultsubsets)
			result = true;
		
		if(!result)
			return false;
		
		if(filter[4] == "X" && obj.type ==
			"regex") return true;
		
		if(filter[5] == "X" && obj.type == "xpath")
			return true;
		
		if(filter[6] == "X" && obj.type == "jsonpointer")
			return true;
		
		if(filter[7] == "X" && obj.type == "jsonpath")
			return true;
		
		return false;
	}
	
	// purge categories completely from configuration
	purgeCategories(removal) {
		// remove from current configuration
		for(var i = this.config_current.length - 1; i >= 0; i--) {
			if(typeof this.config_current[i].cat !== "undefined")
				if($.inArray(this.config_current[i].cat, removal) != -1)
					this.config_current.splice(i, 1);
		}
		
		// remove from configuration read from database
		for(var i = this.config_db.length - 1; i >= 0; i--) {
			if(typeof this.config_db[i].cat !== "undefined")
				if($.inArray(this.config_db[i].cat, removal) != -1)
					this.config_db.splice(i, 1);
		}
	}
}