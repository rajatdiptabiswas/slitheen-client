var page = require('webpage').create(),
	system = require('system'),
	t,address;


if (system.args.length === 1){
	console.log ('Usage: phantomjs --proxy=localhost:8080 --proxy-type=http phantomScript.js <some URL>');
	phantom.exit();
}

t = Date.now();
address = system.args[1];


console.log('Loading page..'+ system.args[1]);

page.open(address, function(status){
	if (status !== 'success'){
		console.log('FAIL to load the address');
		console.log(status);
	}
	else{
		t = Date.now() - t;
		console.log('Loading page..'+ system.args[1]);
		console.log('Loading time: '+ t + 'msec');
	}
	phantom.exit();
});
