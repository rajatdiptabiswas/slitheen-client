liFox Driver

import unittest
from selenium import webdriver
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.firefox.firefox_binary import FirefoxBinary
import time
import random
import numpy

class SliFoxDriver(object):

        def start(self):

                self.driver = webdriver.Firefox(executable_path = "/home/iang/geckodriver")

        def stop(self):
                self.driver.close()

        def read(self):
            body = self.driver.find_elements_by_xpath(".//html")
            #print("BODY: ")
            #print(body)

            body_size = body[0].size['height']
            print("Body size: " + str(body_size))
            read_time = self.random_read(body_size)

            print("Reading for " + str(read_time))
            time.sleep(read_time)

        def navigate_within_site(self, url):


            while True:
                #print("Naviating to link within site")
                links = sfd.driver.find_elements_by_xpath("//a[@href]")
                num_links = len(links)
                #print("Finding a link to nagvigate to ... ")
                rand = random.randint(0, num_links - 1)
                link_url  = links[rand].get_attribute("href")
                if url in link_url:
                    print("Navigating to " + link_url)
                    sfd.driver.get(link_url)
                    break;

        def random_read(self, body_size):
            read_time = random.uniform(0, body_size/300)
            #return reat_time
            return random.randint(5, 15)

if __name__ == "__main__":

        sfd = SliFoxDriver()

        sites = ["https://www.python.org", "https://isocpp.org/"]

        url = random.choice(sites)

        print("Starting SliFoxDriver")
        sfd.start()
        sfd.driver.get(url)
        while True:
            sfd.navigate_within_site(url)
            sfd.read()

