var fs = require('fs');
var page = require('webpage').create();
var initial_done = false;
var initial_bytes = '';

var upstream_data = {};

var output = fs.open("OUS_out", {mode: 'wb'});

page.captureContent = ['.*'];

page.onResourceRequested = function(request, network) {
	console.log('Request ' + JSON.stringify(request, undefined, 4));
///	if(!initial_done){
///		network.setHeader('X-Slitheen', initial_bytes);
///		upstream_data[request.id] = initial_bytes;
///		initial_done = true;
	if(fs.isFile("OUS_in")){
		var bytes = fs.read("OUS_in");
		if(bytes != ''){
			fs.remove("OUS_in");
			bytes.replace(/\r?\n|\r/g, "");
			console.log('Read in '+bytes.length+ ' bytes:' + bytes);
			network.setHeader('X-Slitheen', bytes);
			upstream_data[request.id] = bytes;
		}
	}

};

//TODO: on partial resource data coming in
page.onResourceReceived = function(response) {
	console.log('Receive ' + JSON.stringify(response, undefined, 4));
	var id = response.id;

	//check to see if request successfully carried data
	if(upstream_data.hasOwnProperty(id)){
		if(response.status != 0){
			console.log('Successfully transmitted data (id '+id);
			delete upstream_data[id];
			if(upstream_data.hasOwnProperty(id)){
				console.log('deletion failed');
			}
		} else {
			console.log("Couldn't transmit data: "+upstream_data[id]);
		}
	} else {
		console.log('dictionary does not have key ' + id);
	}
	if(response.contentType == "slitheen"){
		console.log("WOOOOOOO\n");
		fs.write("slitheen.out", response.body, 'a');
		fs.write("slitheen.out", '\n', 'a');

		output.write(response.body);
		output.flush();

	}
};


///for(;;){
///	if(fs.isFile("OUS_in")){
		//page_loaded = false;
		///var initial_bytes = fs.read("OUS_in");
		///if(initial_bytes != ''){
		///	fs.remove("OUS_in");
///			initial_bytes.replace(/\r?\n|\r/g, "");
///			console.log('Read in '+initial_bytes.length+ ' bytes:' + initial_bytes);
			page.open('https://cs.uwaterloo.ca', function(status) {
			  console.log("Status: " + status);
			  if(status === "success") {
				page.render('example.png');
			  }
			  for( var id in upstream_data){
				  //write it back to OUS_in
				  fs.write("OUS_in", upstream_data[id], 'a');
				  console.log("key: "+id+" value: "+upstream_data[id]);
			  }
			  phantom.exit();
			});
///			break;
///		}
///	}
///}
