1. In one tab run: 
python httpProxy.py > consoleLog
# there is a lot of console log generated

2. In second tab run:
./runPhantom.sh

3. Output:
mainData
#contains essential data:
(Request - host - subdomain - GET/POST - resource)
(Response - resource size - resource type)

extraData
#details headers/ cookies etc.

currently i am running a shuffle separately on the list and then rerunning steps 1 and 2
and fixing the script to sort the output to get domain-based, content-type-based  upstream and downstream data

to do: 
to get the sorting done during runtime itself
