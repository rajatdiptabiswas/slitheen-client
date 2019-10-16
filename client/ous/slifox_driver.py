# SliFox Driver

import unittest
# Selenium version 3.14.1
from selenium import webdriver
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.firefox.firefox_binary import FirefoxBinary
from selenium.common.exceptions import TimeoutException
from selenium.webdriver.firefox.options import Options
from selenium.webdriver.common.action_chains import ActionChains
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
#OVERT_SITES = ["https://www.python.org", "https://www.youtube.com/", "https://www.instagram.com/beyonce/", "https://www.instagram.com/taylorswift", "https://www.instagram.com/explore/tags/cats/", "https://www.youtube.com/channel/UC4R8DWoMoI7CAwX8_LjQHig", "https://imgur.com/", "https://www.instagram.com/explore/tags/catsofinstagram/", "https://www.instagram.com/explore/tags/ilovecats/", "https://www.instagram.com/selenagomez/", "https://www.instagram.com/badgalriri", "https://www.instagram.com/arianagrande", "https://www.instagram.com/explore/tags/ootd/", "https://www.instagram.com/explore/tags/food/"]

OVERT_SITES = ["https://b.slitheen.net/r/cats/", "https://b.slitheen.net/r/SupermodelCats/", "https://b.slitheen.net/r/cutecats/", "https://a.slitheen.net", "https://c.slitheen.net"]

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

        self.slifox_driver = SliFoxDriver()
    
    def start(self):
        while True:
            self.dwell()
            self.action()
            
    def stop(self):
        self.slifox_driver.driver.close()
        logging.info("SliFox Diver stopped ...")

    def action(self):
	    # Choose an action using the probability of the action
            action = numpy.random.choice(self.ACTIONS, p=self.ACTION_PROBABILITIES)
            logging.info("Next action: " + action)
            if action == "link":
                self.navigate_within_site()
                return 0
            elif action == "new_addr":	
                self.navigate_to_random_site()
                return 0
            elif action == "history":
                self.navigate_to_history()
                return 0
            elif action == "download":
                self.download()
                return 0
            elif action == "new_tab":
                self.open_tab()
            elif action == "switch_tab":
                self.switch_tab()
            elif action == "close_tab":
                self.close_tab()
            else:
                return -1
    
    def dwell(self):
            dwell_time = WEIBULL_SCALE * numpy.random.weibull(WEIBULL_SHAPE)
            logging.info("Dwelling for " + str(dwell_time))
            time.sleep(dwell_time)

    # Navigate to a link on the same site
    def navigate_within_site(self):
        current_url = self.slifox_driver.driver.current_url
        logging.info("Navigating within site ... ")
        links = self.slifox_driver.driver.find_elements_by_xpath("//a[@href]")
        while True:
            try:
                link_url  = numpy.random.choice(links).get_attribute("href")
            except:			
            # Element may have become stale or some other error occurred
                logging.info("Something was wrong with that link ... trying another one")
                break

            domain = self.get_domain(link_url)

            if domain in current_url:
                logging.debug("Navigating to " + link_url)
                try:
                    logging.debug("Attempting to navigate within site ...")
                    self.slifox_driver.driver.get(link_url)
                    break
                except TimeoutException as tex:
                    logging.info("TimeoutException while navigating within site")

    def navigate_to_random_site(self):
            logging.info("Navigating to new site ... ")
             
            while True:
                new_site = random.choice(OVERT_SITES)
                current_domain = self.get_domain(self.slifox_driver.driver.current_url)
                if new_site not in current_domain:
                    try:
                        logging.info("Attempting to navigate to " + new_site)            	
                        self.slifox_driver.driver.get(new_site)
                        self.save_cookies()
                        break
                    except TimeoutException as tex:
                        logging.info("TimeoutException while trying to navigate to" + str(new_site))


    def navigate_to_site(self, url):
        logging.info("Navigating to " + url)
        
        try:
            self.slifox_driver.driver.get(url)
            self.save_cookies()
        except TimeoutException as tex:
            logging.error("TimeoutException: " + tex)

    def navigate_to_history(self):
        self.slifox_driver.driver.execute_script("window.history.go(-1)")

    def download(self):
        self.dwell()

    def open_tab(self):
        # https://stackoverflow.com/questions/28431765/open-web-in-new-tab-selenium-python
        #https://stackoverflow.com/questions/8833835/python-selenium-webdriver-drag-and-drop
        print("opening tab")
        #ActionChains(self.slifox_driver.driver).key_down(Keys.CONTROL).send_keys('t').key_up(Keys.CONTROL).perform()
        self.slifox_driver.driver.find_element_by_tag_name('body').send_keys(Keys.CONTROL + 't')
        self.slifox_driver.driver.find_element_by_tag_name('body').send_keys(Keys.CONTROL + Keys.TAB)

        windows = self.slifox_driver.driver.window_handles
        self.slifox_driver.driver.switch_to.window(windows[-1])
    #def switch_tab():
        
    def close_tab(self):
        self.slifox_driver.driver.find_element_by_tag_name('body').send_keys(Keys.CONTROL + 'w')

    # Misc helper functions
     
    def get_domain(self, url):
        parsed_url = urlparse(url)
        domain = '{uri.scheme}://{uri.netloc}/'.format(uri=parsed_url)
        return domain

    def save_cookies(self):
        cookies = self.slifox_driver.driver.get_cookies()
        for cookie in cookies:
            self.slifox_driver.driver.add_cookie(cookie)


class YouTubeBackgroundUser(UserModel):
    
    def start(self):
        self.navigate_to_site("https://www.youtube.com/watch?v=QtTh6vgMzAY")
        self.dwell()
        self.open_tab()
        while True:
            self.dwell()
            self.action()



class SliFoxDriver(object):
        def __init__(self):
            fp = webdriver.FirefoxProfile()
            fp.set_preference("browser.tabs.remote.autostart", False)
            fp.set_preference("browser.tabs.remote.autostart.1", False)
            fp.set_preference("browser.tabs.remote.autostart.2", False)

            fp.set_preference("general.useragent.override", "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:60.0) Gecko/20100101 Firefox/68.0")
            logging.info("Starting Slifox Driver ...")
            binary = FirefoxBinary('/home/slitheen/firefox/obj-x86_64-pc-linux-gnu/dist/bin/firefox')
            self.driver = webdriver.Firefox(firefox_profile=fp, firefox_binary = binary, executable_path = "/home/slitheen/client/client/geckodriver-24/geckodriver")
            self.driver.set_page_load_timeout(30)
            logging.info("Slifox Driver started")

        def stop(self):
            logging.info("Shutting down Slifox Driver ...")
            self.driver.close()


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
            if arg == 'background-video':
                user_mode = YouTubeBackgroundUser(ACTION_PROBABILITIES)
            elif arg == 'random':
                user_mode = UserModel(ACTION_PROBABILITIES) 
            else:
                print("Invalid user model")
                sys.exit(0)
        else:
            print("Unknown flag")
            sys.exit(0)

    user_mode.start()
    
