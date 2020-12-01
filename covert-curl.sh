#!/bin/bash

declare -a arr=("https://www.google.com",
		"https://eff.org",
		"https://nytimes.com",
		"https://cbc.ca"
		)
for i in {0..3}
do
	sudo timeout 15 curl --socks5-hostname localhost:1080 ${arr[$i % 4]}
	echo "curl'd ${arr[$i % 4]}"
done
