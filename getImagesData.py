#!/usr/bin/python

import sys
import urllib
#import http.client
import urllib2
import re
from PIL import Image
from io import BytesIO
import cStringIO
#import URLError, HTTPError



urls = ["http://cnn.com"]


i = 0
regex1 = '<title>(.+?)</title>'
pattern1 = re.compile(regex1)
regex2 = '<img src="(.+?)"'
pattern2 = re.compile(regex2)

while i < len(urls):
        htmlfile = urllib.urlopen(urls[i])
        htmltext = htmlfile.read()
        titles = re.findall(pattern1,htmltext)
	images = re.findall(pattern2,htmltext)
	size1 = htmlfile.headers.get("content-length")	
#	size2 = len(htmlfile.read())
        
	print titles
#	print images
	print "content-length of header: "  
	print size1
#	print size2 
	print "-------"
        i+=1

totalImageSize = 0

print "Output:"
#i = 0
#while i < len(images):
#	url = '\''+images[i]+'\''
#		print images[i] 
#		i+=1

#	try:
i = 0
while i < len(images):

		response = urllib.urlopen(images[i])
#	except URLError as e:
#	        print "ERROR: ", e.code()
#	else:
		headers = response.info()
		data = response.read()
		print 'URL: ', response.geturl()
		print 'DATE:', headers['date'] 
		print 'LENGTH:', len(data)
		totalImageSize+=len(data)
		i+=1

print "---------------------"
print "Total bandwidth to download images from %s is %d KB" % (urls, totalImageSize)
