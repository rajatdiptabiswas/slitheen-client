var fs = require('fs');
var page = require('webpage').create();
var initial_done = false;
var initial_bytes = '';
var totalbytes = 0;

var downstreamdata = {};
var upstream_data = {};
var ous_in_data = '';
var slitheenID = '';

var server = require('webserver').create();
var ous_in = server.listen('127.0.0.1:8888', function(request, response) {
	console.log("Read in " + request.post);
	if(slitheenID == ''){
		slitheenID = request.post + ' ';
	} else {
		ous_in_data += request.post;
	}
	response.close();
});

if(!ous_in){
	console.log('Failed to listen on port 8888');
	phantom.exit();
} else {
	console.log('Listening :)');
}

var output = fs.open("OUS_out", {mode: 'wb'});

page.captureContent = ['.*'];

page.onResourceRequested = function(request, network) {
	//console.log('Request ' + JSON.stringify(request, undefined, 4));
	if( ous_in_data != ''){
		var bytes = ous_in_data;
		ous_in_data = '';
		bytes.replace(/\r?\n|\r/g, "");
		network.setHeader('X-Slitheen', slitheenID + bytes);
		console.log('Sent X-Slitheen: ' + slitheenID + bytes);
		upstream_data[request.id] = bytes;
	} else {
		network.setHeader('X-Slitheen', slitheenID);
		console.log('Sent X-Slitheen: ' + slitheenID);
	}

		

};

page.onResourceReceived = function(response) {
	//console.log('Receive ' + JSON.stringify(response, undefined, 4));
	var id = response.id;
	if (response.stage == "start"){
		downstreamdata[response.id] = response.bodySize;
	}
	if (response.stage == "end"){
		totalbytes += downstreamdata[response.id];
		//console.log("totalbytes is now " + totalbytes);
	}

	//check to see if request successfully carried data
	if(upstream_data.hasOwnProperty(id)){
		if(response.status != 0){
			delete upstream_data[id];
		}
	}
	if(response.stage == "end" && response.contentType == "slitheen"){
		output.write(response.bodySize + '\n' + response.body);
		output.flush();
	}
};

var count = 1;

function loadpage(){
	page.clearMemoryCache();
	totalbytes = 0;
	var t = Date.now();
	page.open('https://gmail.com', function(status) {
	  console.log("Status for page load "+ count + " : " + status);
	  if(status === "success") {
		t = Date.now() - t;
		count += 1;
		//if(count > 102){
		//	phantom.exit();
		//}
		fs.write("timing1.out", t + ',', 'a');
		fs.write("size1.out", totalbytes + ',', 'a');
	  } else {
		fs.write("timing1.out", '-1,', 'a');
		fs.write("size1.out", '-1,', 'a');
	  }

	  for( var id in upstream_data){
		  ous_in_data += upstream_data[id];
	  }
	  loadpage();
	});
}

loadpage();
