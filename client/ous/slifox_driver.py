# SliFox Driver

import unittest
# Selenium version 3.14.1
from selenium import webdriver
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.firefox.firefox_binary import FirefoxBinary
from selenium.common.exceptions import TimeoutException
from selenium.webdriver.firefox.options import Options
import time
import random
import numpy
import logging
from urllib.parse import urlparse
import sys
import getopt

# Logging`
LOGFILE = "slifox_driver.log"

# Overt sites list
OVERT_SITES = ["https://www.python.org", "https://www.youtube.com/", "https://www.instagram.com/beyonce/", "https://www.instagram.com/taylorswift", "https://www.instagram.com/explore/tags/cats/", "https://www.youtube.com/channel/UC4R8DWoMoI7CAwX8_LjQHig", "https://imgur.com/", "https://www.instagram.com/explore/tags/catsofinstagram/", "https://www.instagram.com/explore/tags/ilovecats/", "https://www.instagram.com/selenagomez/", "https://www.instagram.com/badgalriri", "https://www.instagram.com/arianagrande", "https://www.instagram.com/explore/tags/ootd/", "https://www.instagram.com/explore/tags/food/"]

# Browser Actions
ACTIONS = ["link", "new_addr", "history", "download", "nop", "new_tab"]
ACTION_PROBABILITIES = [0.451, 0.33, 0.098, 0.012, 0.071, 0.038]

# Dwell time distribution params
WEIBULL_SCALE   = 30
WEIBULL_SHAPE   = 0.75


class UserModel():
    def __init__(self, action_probabilities):
        self.ACTIONS = ACTIONS
        self.ACTION_PROBABILITIES = action_probabilities
        
        # Set up the Selenium Firefox Driver
#        self.driver = SliFoxDriver()
        
        logging.info("Starting SliFox Driver ... ")
        binary = FirefoxBinary('/home/slitheen/firefox/obj-x86_64-pc-linux-gnu/dist/bin/firefox')
        self.driver = webdriver.Firefox(firefox_binary = binary, executable_path = "/home/slitheen/client/client/geckodriver-24/geckodriver")
        self.driver.set_page_load_timeout(30)
        logging.info("SliFox Dirver starter ... ")
    

    def start(self):
        pass
            
    def stop(self):
        self.driver.close()
        logging.info("SliFox Diver stopped ...")

    def action(self):
	    # Choose an action using the probability of the action
            action = numpy.random.choice(self.ACTIONS, p=self.ACTION_PROBABILITIES)
            logging.info("Next action: " + action)
            if action == "link":
                navigate_within_site()
                return 0
            elif action == "new_addr":	
                navigate_to_new_site()
                return 0
            elif action == "history":
                navigate_to_history()
                return 0
            elif action == "download":
                download()
                return 0
            elif action == "new_tab":
                open_tab()
            elif action == "switch_tab":
                switch_tab()
            elif action == "close_tab":
                close_tab()
            else:
                return -1
    
    def dwell(self):
            dwell_time = WEIBULL_SCALE * numpy.random.weibull(WEIBULL_SHAPE)
            logging.info("Dwelling for " + str(dwell_time))
            time.sleep(dwell_time)

    # Navigate to a link on the same site
    def navigate_within_site(elf):
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

    def navigate_to_random_site(self):
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

    def navigate_to_site(self, url):
        logging.info("Navigating to " + url)
        
        try:
            self.driver.get(url)
        except TimeoutException as tex:
            logging.error("TimeoutException: " + tex)

    def navigate_to_history(self):
        self.driver.back()

    def download(self):
        self.dwell()

    def open_tab():
        # https://stackoverflow.com/questions/28431765/open-web-in-new-tab-selenium-python
        self.driver.find_element_by_name('body').send_keys(Keys.CONTROL + 't')

    #def switch_tab():
        
    def close_tab():
        self.driver.find_element_by_name('body').send_keys(Keys.CONTROL + 'w')

    # Misc helper functions
     
    def get_domain(url):
        parsed_url = urlparse(url)
        domain = '{uri.scheme}://{uri.netloc}/'.format(uri=parsed_url)
        return domain


class YouTubeBackgroundUser(UserModel):
    
    def start(self):
        self.navigate_to_new_site("https://www.youtube.com/watch?v=QtTh6vgMzAY")
        self.open_tab()



#class SliFoxDriver(object):
#        def start(self):
#            logging.info("Starting Slifox Driver ...")
#	    binary = FirefoxBinary('/home/iang/firefox/obj-x86_64-pc-linux-gnu/dist/bin/firefox')
            #binary = FirefoxBinary('/home/aemhlori/slitheen-new/slifox/obj-x86_64-pc-linux-gnu/dist/bin/firefox')
#            binary = FirefoxBinary('/home/slitheen/firefox/obj-x86_64-pc-linux-gnu/dist/bin/firefox')
            #opts = Options()
            #opts.headless = True
#            self.driver = webdriver.Firefox(firefox_binary = binary, executable_path = "/home/slitheen/client/client/geckodriver-24/geckodriver")
#            self.driver.set_page_load_timeout(30)
#            logging.info("Slifox Driver started")
#
#        def stop(self):
#            logging.info("Shutting down Slifox Driver ...")
#            self.driver.close()


	# Generate next action
                    # Use Weibull Distribution to generate dwell time
if __name__ == "__main__":
    logging.basicConfig(filename=LOGFILE, level=logging.DEBUG)
     
    try:
        # -u := user mode
        # -h := help
        opts, args = getopt.getopt(sys.argv[1:], "u:h")
    except getopt.GetoptError:
        print("Usage: python slifox_driver.py -u <user_mode>")
        sys.exit(2)

    user_mode = None
    for opt, arg in opts:
        if opt == "-h":
            print("Usage: python slifox_driver.py -u <user_mode>")
            sys.exit(0)
        elif opt == "-u":
            user_mode = arg
        else:
            print("Unknown flag")
            sys.exit(0)


    user = None
    if user_mode == 'test':
        user = YouTubeBackgroundUser(ACTION_PROBABILITIES)

    while True:
        user.dwell()
        user.action()
