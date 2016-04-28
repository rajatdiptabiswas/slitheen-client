#!/bin/bash

count=0
while true
do
	count=$((count+1))
	echo "Calling phantomJS fr the $count th time"
	./phantomjs --ssl-callbacks=slitheen --ssl-protocol=tlsv1.2 --ssl-ciphers=DHE-RSA-AES256-GCM-SHA384 loadpage.js > out-$count-put.out
done

