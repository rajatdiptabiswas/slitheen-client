# SliFox Driver

import unittest
from selenium import webdriver
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.firefox.firefox_binary import FirefoxBinary
from selenium.common.exceptions import TimeoutException
import time
import random
import numpy
import logging
from urlparse import urlparse

# Logging`
LOGFILE = "slifox_driver.log"

# Overt sites list
OVERT_SITES = ["https://www.python.org", "https://www.youtube.com/"]

# Browser Actions
ACTIONS = ["link", "new_addr", "history", "download", "nop"]
ACTION_PROBABILITIES = [0.451, 0.33, 0.098, 0.012, 0.109]

# Dwell time distribution params
WEIBULL_SCALE   = 30
WEIBULL_SHAPE   = 0.75

class SliFoxDriver(object):

        def start(self):
		logging.info("Starting Slifox Driver ...")
		binary = FirefoxBinary('/home/iang/firefox/obj-x86_64-pc-linux-gnu/dist/bin/firefox')
                self.driver = webdriver.Firefox(firefox_binary = binary, executable_path = "/home/iang/geckodriver")
		self.driver.set_page_load_timeout(30)
		logging.info("Slifox Driver started")

        def stop(self):
		logging.info("Shutting down Slifox Driver ...")
                self.driver.close()


	# Generate next action
	def action(self):
		# Choose an action using the probability of the action
		action = numpy.random.choice(ACTIONS, p=ACTION_PROBABILITIES)
		logging.info("Next action: " + action)
		if action == "link":
			self.navigate_within_site()
			return 0
		elif action == "new_addr":	
			self.navigate_to_new_site()
			return 0
		elif action == "history":
			self.navigate_to_history()
			return 0
		elif action == "download":
			self.download()
			return 0
		else:
			return -1

	# Use Weibull Distribution to generate dwell time
	def dwell(self):
		dwell_time = WEIBULL_SCALE * numpy.random.weibull(WEIBULL_SHAPE)
		logging.info("Dwelling for " + str(dwell_time))
		time.sleep(dwell_time)

 	# Navigate to a link on the same site
 	def navigate_within_site(self):
 		current_url = self.driver.current_url
		logging.info("Navigating within site ... ")
 		links = self.driver.find_elements_by_xpath("//a[@href]")
		while True:
 			try:
				link_url  = numpy.random.choice(links).get_attribute("href")
 			except:			
				# Element may have become stale or some other error occurred
				logging.info("Something was wrong with that link ... trying another one")
				break

			domain = get_domain(link_url)
 
 			if domain in current_url:
 				logging.debug("Navigating to " + link_url)
 				try:
					logging.debug("Attempting to navigate within site ...")
 					self.driver.get(link_url)
 					break
 				except TimeoutException as tex:
 					logging.info("TimeoutException while navigating within site")
        
	def navigate_to_new_site(self):
		logging.info("Navigating to new site ... ")
		 
		while True:
			new_site = random.choice(OVERT_SITES)
			current_domain = get_domain(self.driver.current_url)
			if new_site not in current_domain:
				try:
					logging.info("Attempting to navigate to " + new_site)            	
					self.driver.get(new_site)
					break
				except TimeoutException as tex:
					logging.info("TimeoutException while trying to navigate to a new site")


	def navigate_to_history(self):
		self.driver.back()

	def download(self):
		self.dwell()

# Misc helper functions
 
def get_domain(url):
	parsed_url = urlparse(url)
	domain = '{uri.scheme}://{uri.netloc}/'.format(uri=parsed_url)
	return domain

if __name__ == "__main__":
	logging.basicConfig(filename=LOGFILE, level=logging.DEBUG)
        sfd = SliFoxDriver()

        url = random.choice(OVERT_SITES)
        sfd.start()
	logging.info("Getting initial URL")
        sfd.driver.get(url)
        while True:
		sfd.dwell()
		sfd.action()
