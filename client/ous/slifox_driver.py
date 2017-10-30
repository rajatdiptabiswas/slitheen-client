# SliFox Driver

import unittest
from selenium import webdriver
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.firefox.firefox_binary import FirefoxBinary
import time
import random
import numpy
import logging

# Logging`
LOGFILE = "slifox.log"

WEIBULL_SCALE   = 30
WEIBULL_SHAPE   = 0.75

class SliFoxDriver(object):

        def start(self):
		logging.info("Starting Slifox Driver ...")
		logging.info("Getting FF webdriver ... ")
		binary = FirefoxBinary('/home/iang/firefox/obj-x86_64-pc-linux-gnu/dist/bin/firefox')
                self.driver = webdriver.Firefox(firefox_binary = binary, executable_path = "/home/iang/geckodriver")

        def stop(self):
		logging.info("Shutting down Slifox Driver ...")
                self.driver.close()

        def read(self):
            #body = self.driver.find_elements_by_xpath(".//html")

            #body_size = body[0].size['height']
            read_time = self.random_read()

            logging.debug("Reading for " + str(read_time))
            time.sleep(read_time)

        def navigate_within_site(self, url):
	    print("Navigating ...")

            while True:
                links = sfd.driver.find_elements_by_xpath("//a[@href]")
                num_links = len(links)
                rand = random.randint(0, num_links - 1)
                link_url  = links[rand].get_attribute("href")
                if url in link_url:
                    logging.debug("Navigating to " + link_url)
                    sfd.driver.get(link_url)
                    break;

        def random_read(self):
            dwell_time = numpy.random.weibull(WEIBULL_SHAPE*WEIBULL_SCALE)    
            return dwell_time

if __name__ == "__main__":
	logging.basicConfig(filename=LOGFILE, level=logging.DEBUG)
        sfd = SliFoxDriver()

        sites = ["https://www.reddit.com/", "https://www.python.org"]

        url = random.choice(sites)

        sfd.start()
        sfd.driver.get(url)
        while True:

            sfd.navigate_within_site(url)
            sfd.read()

