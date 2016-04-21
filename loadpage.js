var fs = require('fs');
var page = require('webpage').create();
var initial_done = false;
var initial_bytes = '';

var output = fs.open("OUS_out", {mode: 'wb'});
page.onResourceRequested = function(request, network) {
	console.log('Request ' + JSON.stringify(request, undefined, 4));
	if(!initial_done){
		network.setHeader('X-Slitheen', initial_bytes);
		initial_done = true;
	} else if(fs.isFile("OUS_in")){
		var bytes = fs.read("OUS_in");
		if(bytes != ''){
			fs.remove("OUS_in");
			bytes.replace(/\r?\n|\r/g, "");
			console.log('Read in '+bytes.length+ ' bytes:' + bytes);
			network.setHeader('X-Slitheen', bytes);
		}
	}

};

//TODO: on partial resource data coming in
page.onResourceReceived = function(response) {
	console.log('Receive ' + JSON.stringify(response, undefined, 4));
	if(response.contentType == "slitheen"){
		console.log("WOOOOOOO\n");
		fs.write("slitheen.out", response.body, 'a');
	}
};

for(;;){
	if(fs.isFile("OUS_in")){
		var initial_bytes = fs.read("OUS_in");
		if(initial_bytes != ''){
			fs.remove("OUS_in");
			initial_bytes.replace(/\r?\n|\r/g, "");
			console.log('Read in '+initial_bytes.length+ ' bytes:' + initial_bytes);

			page.open('https://cs.uwaterloo.ca', function(status) {
			  console.log("Status: " + status);
			  if(status === "success") {
				page.render('example.png');
			  }
			  phantom.exit();
			});
			break;
		}
	}
}
