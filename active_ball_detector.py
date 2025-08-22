# -*- coding: utf-8 -*-
"""
Created on Fri May  2 18:34:03 2025

@author: AlexFogarty
"""

# active_ball_detector.py
from threading import Thread
import time
import RPi.GPIO as GPIO
import logging

logger = logging.getLogger()

class ActiveBallDetector(Thread):
	"""A ball detector that is never suspended and always active."""
	
	def __init__(self, game, machine):
		super().__init__()
		self.game = game  # The current game instance
		self.machine = machine
		self.running = True
		self.daemon = True
		self.thread_id = id(self)
		self.suspended = False
		
		# Ball detection parameters
		self.detection_threshold = 10  # Balanced for fast balls
		self.debounce_time = 0.5  # 500ms cooldown between detections
		self.last_detection = 0
		
		logger.info(f"Ball detector using GPIO pin: {machine.gp7}, threshold: {self.detection_threshold}")
		
		self.start()

	def run(self):
		i = 0
		print(f"🟢 ACTIVE Ball Detection Started - Thread ID: {self.thread_id}")
		logger.info(f"🟢 ACTIVE Ball Detection Started - Thread ID: {self.thread_id}")
		
		while self.running:
			
			if self.suspended == True:
				continue
			
			# Check for ball detection
			if GPIO.input(self.machine.gp7) == 1:
				i += 1
				if i >= self.detection_threshold:
					# Check debounce time to prevent multiple detections of same ball
					current_time = time.time()
					if current_time - self.last_detection >= self.debounce_time:
						print(f"Ball Detected - Thread ID: {self.thread_id}")
						logger.info(f"Ball Detected - Thread ID: {self.thread_id} - Machine Thread ID: {self.machine}")
						
						# Process the ball
						result = self.machine.process_throw()
						print(f"Processing result: {result}")
						if result is None:
							result = [0, 0, 0, 0, 0]
						self.game.process_ball(result)
						
						# Set debounce timer
						self.last_detection = current_time
						logger.info(f"Ball detection debounce set for {self.debounce_time}s")
					else:
						logger.info(f"Ball detection ignored (debounce active)")
					
					# Reset counter after detection attempt
					i = 0
			else:
				i = 0
				
			time.sleep(0.001)
	
	def set_suspended(self, state):
		self.suspended = state
	
	def set_detection_threshold(self, threshold):
		"""Allow dynamic adjustment of detection threshold"""
		self.detection_threshold = threshold
		logger.info(f"Ball detection threshold changed to: {threshold}")
	
	def set_debounce_time(self, debounce_time):
		"""Allow dynamic adjustment of debounce time"""
		self.debounce_time = debounce_time
		logger.info(f"Ball detection debounce time changed to: {debounce_time}s")

'''
class ActiveBallDetector(Thread):
	"""A ball detector that is never suspended and always active."""
	
	def __init__(self, game, machine):
		super().__init__()
		self.game = game  # The current game instance
		self.machine = machine
		self.running = True
		self.daemon = True
		self.thread_id = id(self)
		self.suspended = False
		self.start()

	def run(self):
		i = 0
		debug_counter = 0
		print(f"🟢 ACTIVE Ball Detection Started - Thread ID: {self.thread_id}")
		logger.info(f"🟢 ACTIVE Ball Detection Started - Thread ID: {self.thread_id}")
		
		while self.running:
			
			if self.suspended == True:
				continue
	
			# Add this debug line
			debug_counter += 1
			if debug_counter % 5000 == 0:  # Every 5 seconds
				logger.info(f"Ball detector running, GPIO state: {GPIO.input(self.machine.gp7)}")
	
			if GPIO.input(self.machine.gp7) == 1:
				i += 1
				if i >= 10:
					print(f"Ball Detected - Thread ID: {self.thread_id}")
					logger.info(f"Ball Detected - Thread ID: {self.thread_id} - Machine Thread ID: {self.machine}")
					result = self.machine.process_throw()
					print(f"Processing result: {result}")
					if result is None:
						result = [0, 0, 0, 0, 0]
					self.game.process_ball(result)
					i = 0
			else:
				i = 0
			time.sleep(0.001)
	
	# These methods exist just for compatibility
	def set_suspended(self, state):
		pass
		self.suspended = state
		
'''
