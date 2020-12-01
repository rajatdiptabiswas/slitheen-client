# SliFox Driver
# TODO Fix up logging
# TODO Fix up comments
# TODO Remove javascript console logs

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

import yt_parser as ytp

# Logging`
LOGFILE = "slifox_driver.log"

# Overt sites list
TEST_SITES = ["https://www.reddit.com","https://www.cbc.ca", "https://www.canadaland.com"]

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
        
        self.slifox_driver = SliFoxDriver()
    
    def start(self):
        self.navigate_to_random_site()
        while True:
            self.action()
            self.dwell()
            
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
                    self.slifox_driver.driver.get(link_url)
                    break
                except Exception as ex:
                    logging.error("Exception while navigating within site: " + str(ex))

    def navigate_to_random_site(self):
            logging.info("Navigating to random site ... ")
             
            while True:
                new_site = random.choice(TEST_SITES)
                current_domain = self.get_domain(self.slifox_driver.driver.current_url)
                if new_site not in current_domain:
                    try:
                        logging.info("Navigating to " + new_site)            	
                        self.slifox_driver.driver.get(new_site)
                        self.save_cookies()
                        break
                    except Exception as ex:
                        logging.error("Exception while trying to navigate to" + str(new_site) + ": " + str(ex))


    def navigate_to_site(self, url):
        logging.info("Navigating to " + url)
        
        try:
            self.slifox_driver.driver.get(url)
            self.save_cookies()
        except Exception as ex:
            logging.error("Exception while trying to navigate to" + str(url) + ": " + str(ex))

    def navigate_to_history(self):
        self.slifox_driver.driver.execute_script("window.history.go(-1)")

    def download(self):
        self.dwell()

    def open_tab(self):
        # https://stackoverflow.com/questions/28431765/open-web-in-new-tab-selenium-python
        #https://stackoverflow.com/questions/8833835/python-selenium-webdriver-drag-and-drop
        logging.info("Opening tab")
        #ActionChains(self.slifox_driver.driver).key_down(Keys.CONTROL).send_keys('t').key_up(Keys.CONTROL).perform()
        try:
            self.slifox_driver.driver.find_element_by_tag_name('body').send_keys(Keys.CONTROL + 't')
            self.slifox_driver.driver.find_element_by_tag_name('body').send_keys(Keys.CONTROL + Keys.TAB)
            windows = self.slifox_driver.driver.window_handles
            self.slifox_driver.driver.switch_to.window(windows[-1])

        except Exception as e:
            logging.error(e)

        
    def close_tab(self):
        self.slifox_driver.driver.find_element_by_tag_name('body').send_keys(Keys.CONTROL + 'w')

    # Misc helper functions
     
    def get_domain(self, url):
        parsed_url = urlparse(url)
        domain = '{uri.scheme}://{uri.netloc}/'.format(uri=parsed_url)
        return domain

    def save_cookies(self):
        logging.info("Saving cookies")
        cookies = self.slifox_driver.driver.get_cookies()
        for cookie in cookies:
            self.slifox_driver.driver.add_cookie(cookie)


class BasicModel(UserModel):
    '''
    This user model loads the same site every 10 seconds.
    '''

    def start(self):
        logging.info("Starting basic mode ... ")
        while True:
           self.navigate_to_site("https://www.google.com/search?site=&tbm=isch&q=cats") # AL - Can be changed
           time.sleep(10)
	
class BackgroundVideoUser(UserModel):
    '''
    This user model uses two tabs to simulate a user.
    The first tab is a never ending YouTube live stream,
    the other follow the base UserModel.
    '''
    def start(self):
        logging.info("Starting background video ...")
        self.navigate_to_site("https://www.youtube.com/watch?v=QtTh6vgMzAY") # This is the neverending bob's burgers live stream
        self.dwell()
        self.open_tab()
        while True:
            self.dwell()
            self.action()



class VideoUser(UserModel):
    '''
    This user model uses one tab to watch YouTube videos.
    Notably, this user model can detect when a video finnishes
    and will click on a recommended video.
    '''

    def start(self):
        self.navigate_to_site("https://www.youtube.com")
#        self.dwell()
        
        # Navigate to first video
        self.navigate_to_site(self.slifox_driver.ytp.home_page_select())
        self.slifox_driver.ytp.disable_autoplay()
        #self.slifox_driver.ytp.click_play()  

        while True:
            self.slifox_driver.ytp.click_play()
            self.slifox_driver.ytp.detect_video_ended()
            self.dwell()
            self.navigate_to_site(self.slifox_driver.ytp.secondary_recommendations_select())

class SliFoxDriver(object):
        def __init__(self):
            fp = webdriver.FirefoxProfile()
            
            # Enable opening tabs
            fp.set_preference("browser.tabs.remote.autostart", False)
            fp.set_preference("browser.tabs.remote.autostart.1", False)
            fp.set_preference("browser.tabs.remote.autostart.2", False)

            # Set user agent
            fp.set_preference("general.useragent.override", "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:60.0) Gecko/20100101 Firefox/60.0")
            # Disable HTTP/2 and TLS1.3
            fp.set_preference("security.tls.version.max", 3)
            fp.set_preference("network.http.spdy.enabled.http2", False)

            # Get Slifox Binary
            binary = FirefoxBinary('/home/slitheen/firefox/obj-x86_64-pc-linux-gnu/dist/bin/firefox')
            
            # Instantiate driver with the correct geckodriver version
            self.driver = webdriver.Firefox(firefox_profile=fp, executable_path = "/home/slitheen/client/client/geckodriver-26/geckodriver", firefox_binary = binary) 
            
            # Set page and script timeouts
            self.driver.set_page_load_timeout(30)
            self.driver.set_script_timeout(50000000)

            # Set up Youtube Parser
            self.ytp = ytp.YoutubeParser(self.driver)
            
            logging.info("Slifox Driver started")

        def stop(self):
            logging.info("Shutting down Slifox Driver ...")
            self.driver.close()

def print_usage():
    print("Usage: python slifox_driver.py -h | -u <user_mode>")

if __name__ == "__main__":
    logging.basicConfig(filename=LOGFILE, format='%(asctime)s.%(msecs)03d %(levelname)s %(module)s - %(funcName)s: %(message)s', level=logging.DEBUG, datefmt='%Y-%m-%d %H:%M:%S') 
   
    logging.info("<<<<< STARTING >>>>>")	  
    try:
	# -t := test mode (uses test overt sites, not the slitheen.net ones)
        # -u := user mode
        # -h := help
        opts, args = getopt.getopt(sys.argv[1:], "u:h")
    except getopt.GetoptError:
        print_usage()
        sys.exit(2)
    
    if len(opts) == 0:
        print_usage()
        sys.exit(2)

    user_mode = None
    for opt, arg in opts:
        if opt == "-h":
            print_usage()
            sys.exit(0)
        elif opt == "-u":
            if arg == 'bv':
                user_mode = BackgroundVideoUser(ACTION_PROBABILITIES)
            elif arg == 'video':
                user_mode = VideoUser(ACTION_PROBABILITIES)
            elif arg == 'random':
                user_mode = UserModel(ACTION_PROBABILITIES) 
            elif arg == 'basic':
                user_mode = BasicModel(ACTION_PROBABILITIES)
            else:
                print("Invalid user model")
                sys.exit(0)
        else:
            print("Unknown flag: " + opt)
            sys.exit(2)

    user_mode.start()
    
