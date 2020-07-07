#!/bin/bash

sudo apt-get -y install curl
sudo rm -f /usr/local/lib/libssl.so.1.1
sudo rm -f /usr/local/lib/libcrypto.so.1.1
declare -a arr=("https://www.google.com",
		"https://eff.org",
		"https://nytimes.com",
		"https://cbc.ca"
		)
for i in {0..300}
do
	sudo timeout 30 curl --socks5-hostname localhost:1080 ${arr[$i % 4]}
	echo "curl'd ${arr[$i % 4]}"
done
