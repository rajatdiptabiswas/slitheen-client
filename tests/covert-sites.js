/*
 * Slitheen - a decoy routing system for censorship resistance
 * Copyright (C) 2017 Cecylia Bocovich (cbocovic@uwaterloo.ca)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Additional permission under GNU GPL version 3 section 7
 * 
 * If you modify this Program, or any covered work, by linking or combining
 * it with the OpenSSL library (or a modified version of that library), 
 * containing parts covered by the terms of the OpenSSL Licence and the
 * SSLeay license, the licensors of this Program grant you additional
 * permission to convey the resulting work. Corresponding Source for a
 * non-source form of such a combination shall include the source code
 * for the parts of the OpenSSL library used as well as that of the covered
 * work.
 */
var fs = require('fs');

var page = require('webpage').create();

page.settings.resourceTimeout = 50000;

page.onResourceRequested = function(request, network) {
    console.log('Request ' + JSON.stringify(request, undefined, 4));

};

page.onResourceReceived = function(response) {
    console.log('Receive ' + JSON.stringify(response, undefined, 4));
};

var stream = fs.open('top100.txt', 'r');

function loadpage(){

    //ping port 8888 to mark end of page
    var url = "http://localhost:8888";
    page.open(url, function (status) {
        
    

    var line = stream.readLine();
    console.log(line);

    page.clearMemoryCache();
    var t = Date.now();
    page.open(line, function(status) {
        if(status === "success") {
            t = Date.now() - t;
            console.log("page load time: "+ t);
            fs.write("timing.out", line + ','+ t + '\n', 'a');
        } else {
            console.log("page load failed");
            fs.write("timing.out", line + ','+ '-1\n', 'a');
        }

        if(!stream.atEnd()){
            loadpage();
        } else {
            phantom.exit();
        }


    });
    });

}

loadpage();
