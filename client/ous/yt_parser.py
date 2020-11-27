# This is the Youtube Parser module for the Overt User Simulator

from slifox_driver import SliFoxDriver
import numpy
import logging
from selenium.webdriver.common.action_chains import ActionChains

class YoutubeParser():
    ''' This classes handles parsing Youtube so that it can be navigated.
    inherits: SlifoxDriver
    '''
    def __init__(self, driver):
        self.driver = driver

    def home_page_select(self):
        ''' This function selects a video to watch from the YouTube home page.
        input: None
        return: URL of selected video
         '''
        logging.info("Selecting video on Youtube home page")
        video_elmts = self.driver.find_elements_by_xpath("//div[@id='meta']")
        while True:
            elmt = numpy.random.choice(video_elmts)
            live = elmt.find_elements_by_class_name("badge-style-type-live-now")
            link = elmt.find_elements_by_id("video-title-link")
            # Exclude videos that are currently live streaming
            if (not live) and link:
                print(elmt)
                url = link[0].get_attribute('href')
                return url

    def secondary_recommendations_select(self):
        ''' This funtion selects a video from the secondary recommendations on a video page.
        input: None
        return: URL of selected video
        '''
        logging.info("Selecting video from recommendations")
        video_elmts = self.driver.find_elements_by_class_name("ytd-watch-next-secondary-results-renderer")
        while True:
            elmt = numpy.random.choice(video_elmts)
            live = elmt.find_elements_by_class_name("bad-style-type-live-now")
            link = elmt.find_elements_by_id('thumbnail')
            if (not live) and link:
                url = link[0].get_attribute('href')
                return url

    def detect_video_ended(self):
        ''' This function detects when a video is finished playback.
        input: None
        output: None
        '''
        script = """ var done = arguments[0];
                     
                     var observer = new MutationObserver(classChangedCallback);
                     observer.observe(document.getElementById('movie_player'), {
                        attributes: true,
                        attributeFilter: ['class'],
                    });

                    function classChangedCallback(mutations, observer) {
                        console.log("ugh");
                        var newIndex = mutations[0].target.className;
                        console.log(newIndex.indexOf('ended-mode'));
                        if ((newIndex.indexOf('ended-mode') != -1) && (newIndex.indexOf('ad-showing') == -1)) {
                            console.log('video ended');
                            observer.disconnect();
                            done("foo")
                    }
                }"""
        ret = self.driver.execute_async_script(script)
        logging.info("Detected that video finished")

    def disable_autoplay(self):
        ''' This function disables the autoplay feature.
        input: None
        output: None
        '''
        try:
            elmt = self.driver.find_element_by_xpath("//div[@id='toggleButton']")
            actions = ActionChains(self.driver)
            actions.move_to_element(elmt).perform()
            elmt.click()
            logging.info("Disabled Autoplay")
        except Exception as ex:
            logging.error("Unable to disable autoplay: " + str(ex))
            pass
       
    def click_play(self):
        while True:
            try:
                self.driver.find_element_by_xpath("//button[@class='ytp-play-button ytp-button']").click()
            except Exception as ex:
                logging.info(ex)
                continue
            break

