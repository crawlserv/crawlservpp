/**
 * Algorithms class for the crawlserv frontend
 */

// @required: helper function msToStr() from frontend.js!

class Algo {
	/*
	 * GENERAL FUNCTIONS
	 */
	
	// constructor: get current algorithm category, id and create list of algorithms
	constructor(newCat, newAlgo, callback_when_finished) {		
		// set filter to show no options
		this.config_cats = [];
		
		// save start time
		var startTime = +new Date();
		
		// save class pointer
		let thisClass = this;
		
		// parse data from JSON file for algorithms
		$.getJSON("json/algos.json", function(data) {
			var id = 0, cat = 0;
			
			if(newAlgo != null) {
				// overwrite algorithm id: get new algorithm category
				id = newAlgo;
				var cat_count = 0, found = false;
				for(var cat_key in data) {
					if(data.hasOwnProperty(cat_key)) {
						for(var algo_key in data[cat_key]) {
							if(data[cat_key][algo_key]["algo-id"] == id) {
								cat = cat_count;
								found = true;
								break;
							}
						}
						if(found) break;
						cat_count++;
					}
				}
				
			}
			else if(newCat != null) {
				// overwrite category id
				cat = newCat;
			}
			else {			
				// get current algorithm id (if specified)
				for(var key in db_config) {
					if(db_config[key].name == "_algo") {
						id = db_config[key].value;
						break;
					}
				}
				
				if(id) {
					// get current algorithm category
					var cat_count = 0, found = false;
					for(var cat_key in data) {
						if(data.hasOwnProperty(cat_key)) {
							for(var algo_key in data[cat_key]) {
								if(data[cat_key][algo_key]["algo-id"] == id) {
									cat = cat_count;
									found = true;
									break;
								}
							}
							if(found) break;
							cat_count++;
						}
					}
				}
			}
			
			// add categories
			var catitems = "", algoitems = "", catcounter = 0;
			$.each(data, function(cat_key, cat_value) {
			    catitems += "<option value='" + catcounter + "'";
			    if(catcounter == cat) {
			    	// found current category: add algorithms
			    	$.each(cat_value, function(algo_key, algo_value) {
			    		var algo_id = 0, algo_config_cats = [];
			    		$.each(algo_value, function(opt_key, opt_value) {
			    			if(opt_key == "algo-id") algo_id = opt_value;
			    			else if(opt_key == "config-cats") algo_config_cats = opt_value;
			    		});
			    		algoitems += "<option value='" + algo_id + "'"
			    		
			    		// if no algorithm has been selected, select first algorithm
			    		if(!id) id = algo_id;
			    		
			    		if(algo_id == id) {
			    			// found current algorithm: set filter to algorithm categories (and the general analyzer options)
			    			algo_config_cats.unshift("general");
			    			thisClass.config_cats = algo_config_cats;
			    			
			    			// select current algorithm
			    			algoitems += " selected";
			    		}
			    		algoitems += ">" + algo_key + "</option>";
			    	});
			    	
			    	// select current category
			    	catitems += " selected";
			    }
			    catitems += ">" + cat_key + "</option>";
			    catcounter++;
			});
			
			if(!catitems.length) catitems = "<option disabled>No categories available</option>";
			if(!algoitems.length) algoitems = "<option disabled>No algorithms available</option>";
				
			$('#algo-cat-select').append(catitems);
			$('#algo-select').append(algoitems);
			
			// save id of selected algorithm
			this.id = id;
			
			// success!
			var endTime = +new Date();
			console.log("Algo::constructor(): Algorithm data loaded after " + msToStr(endTime - startTime) + ".");
			callback_when_finished(id);
		})
		.fail(function(jqXHR, textStatus, errorThrown) {
			throw new Error("Algo::constructor(): Could not load algorithm data.\n" + textStatus + ": " + errorThrown);
		});
	}
	
	/*
	 * DATA FUNCTIONS
	 */
	
	// get id of selected algorithm
	getId() {
		return this.id;
	}
	
	// get categories of current algorithm
	getConfigCats() {
		return this.config_cats;
	}
	
}