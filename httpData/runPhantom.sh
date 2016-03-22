for i in `cat top5k`
do
phantomjs --proxy=localhost:8080 --proxy-type=http --ssl-protocol=any --ignore-ssl-errors=true phantomScript.js $i
done
