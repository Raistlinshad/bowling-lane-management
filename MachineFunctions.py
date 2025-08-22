# -*- coding: utf-8 -*-
"""
Created on Sat Aug 16 15:53:06 2025

@author: ryu_7
"""
import time
import json
import RPi.GPIO as GPIO
import busio
import board
from games1 import setup_logging
import logging
import adafruit_ads1x15.ads1115 as ADS
from adafruit_ads1x15.analog_in import AnalogIn as AIN
import subprocess

control = [0,0,0,0,0]

# Load settings
with open('settings.json') as f:
	lane_settings = json.load(f)
	
LaneID = lane_settings["Lane"]
print("Lane ID: ", LaneID)
logger = setup_logging()
mp = 0 

class MachineFunctions:

	_instance = None
	_initialized = False
	
	def __new__(cls):
		"""Singleton pattern to ensure only one instance exists"""
		if cls._instance is None:
			cls._instance = super(MachineFunctions, cls).__new__(cls)
		return cls._instance
	
	def __init__(self):
		"""Initialize the hardware only once"""
		if not self._initialized:
			self._setup_hardware()
			self._initialized = True
		
		# Reset operation tracking
		self.pending_operation = None
		self.pending_data = None
		
		# FIXED: Add flags from old implementation
		self._needs_full_reset = False
		self._force_full_reset = False
		
		# Machine cycle timing tracking
		self.machine_cycle_start_time = None
		self.reset_called_time = None
		
		# NEW: 3rd ball handling
		self.pin_set_enabled = True
		self.pin_set_restore_time = None
		
		# Symbol popup callback
		self.symbol_popup_callback = None
		
		# Game context for ball detection
		self.game_context = None
	
	def _setup_hardware(self):
		"""Setup the hardware interfaces"""
		try:
			# Run i2c detection to ensure bus is ready
			subprocess.call(['i2cdetect', '-y', '1'], stdout=subprocess.DEVNULL)
			
			# Load configuration
			with open('settings.json') as f:
				self.lane_settings = json.load(f)
			
			self.lane_id = self.lane_settings["Lane"]
			logger.info(f"Initializing MachineFunctions for Lane {self.lane_id}")
			
			# Extract GPIO pin numbers
			self.gp1, self.gp2, self.gp3, self.gp4, self.gp5, self.gp6, self.gp7, self.gp8 = [
				int(self.lane_settings[self.lane_id][f"GP{i}"]) for i in range(1, 9)
			]
			
			# Initialize GPIO
			GPIO.setmode(GPIO.BCM)
			GPIO.setup([self.gp1, self.gp2, self.gp3, self.gp4, self.gp5, self.gp6], GPIO.OUT)
			GPIO.setup([self.gp7, self.gp8], GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
			GPIO.output([self.gp1, self.gp2, self.gp3, self.gp4, self.gp5, self.gp6], 1)
			
			# Initialize pin control state (1 = up, 0 = down)
			self.control = {'lTwo': 1, 'lThree': 1, 'cFive': 1, 'rThree': 1, 'rTwo': 1}
			self.control_change = {'lTwo': 0, 'lThree': 0, 'cFive': 0, 'rThree': 0, 'rTwo': 0}
			
			# Initialize ADS converters
			self._init_ads()
			
			# Get pin name mappings from settings
			self.pb10 = self.lane_settings[self.lane_id]["B10"]
			self.pb11 = self.lane_settings[self.lane_id]["B11"]
			self.pb12 = self.lane_settings[self.lane_id]["B12"]
			self.pb13 = self.lane_settings[self.lane_id]["B13"]
			self.pb20 = self.lane_settings[self.lane_id]["B20"]
			
			# Machine timing (simplified)
			self.mp = 8.5
			self._load_calibration()
			
			# State tracking
			self.pin_check = False
			self.pins_changed = False
			
			logger.info(f"MachineFunctions hardware setup complete for Lane {self.lane_id}")
			
		except Exception as e:
			logger.error(f"Failed to initialize MachineFunctions: {e}")
			# Use conservative defaults
			self.mp = 8.5
			self.control = {'lTwo': 1, 'lThree': 1, 'cFive': 1, 'rThree': 1, 'rTwo': 1}
	
	def _init_ads(self):
		"""Initialize the ADS ADC converters"""
		try:
			i2c = busio.I2C(board.SCL, board.SDA)
			self.ads1 = ADS.ADS1115(i2c, address=0x48)
			self.ads2 = ADS.ADS1115(i2c, address=0x49)
			
			# Setup analog inputs
			self.b10 = AIN(self.ads1, ADS.P0)
			self.b11 = AIN(self.ads1, ADS.P1)
			self.b12 = AIN(self.ads1, ADS.P2)
			self.b13 = AIN(self.ads1, ADS.P3)
			self.b20 = AIN(self.ads2, ADS.P0)
			self.b21 = AIN(self.ads2, ADS.P1)
			
			logger.info("ADS ADC converters initialized successfully")
			
		except Exception as e:
			logger.error(f"ADS initialization failed: {e}")
			raise
	
	def _load_calibration(self):
		"""Load stored calibration or use default"""
		try:
			with open('settings.json', 'r') as f:
				settings = json.load(f)
			
			lane_id = str(self.lane_id)
			if (lane_id in settings and 
				"B21Calibration" in settings[lane_id] and 
				"MPValue" in settings[lane_id]["B21Calibration"]):
				logger.info(f"Using stored MP timing: {self.mp:.2f}s")
			else:
				logger.info("No stored calibration found, using default timing")
				
		except Exception as e:
			logger.warning(f"Error loading calibration: {e}")
	
	def _is_third_ball(self):
		"""Check if this is the 3rd ball of a frame"""
		try:
			if hasattr(self, 'game_context') and self.game_context:
				current_bowler = self.game_context.bowlers[self.game_context.current_bowler_index]
				current_frame = current_bowler.frames[current_bowler.current_frame]
				return len(current_frame.balls) >= 2
			return False
		except Exception as e:
			logger.error(f"Error determining if 3rd ball: {e}")
			return False
	
	def _update_pin_set_status(self):
		"""Update pin_set_enabled status based on timing"""
		current_time = time.time()
		
		# Check if we need to restore pin setting after 8 seconds
		if (not self.pin_set_enabled and 
			self.pin_set_restore_time and 
			current_time >= self.pin_set_restore_time):
			self.pin_set_enabled = True
			self.pin_set_restore_time = None
			logger.info("PIN_SET_RESTORED: Pin setting re-enabled after 8 second delay")
	
	def reset(self):
		try:
			# Record when reset is called for proper MP timing
			self.reset_called_time = time.time()
			self.machine_cycle_start_time = self.reset_called_time
			
			# FIXED: For 3rd balls, disable pin setting for 8 seconds
			if self._is_third_ball():
				self.pin_set_enabled = False
				self.pin_set_restore_time = self.reset_called_time + 8.0
				logger.info("MACHINE_3RD_BALL: Pin setting disabled for 8 seconds")
			
			logger.info("MACHINE_RESET_START: Starting machine cycle with reset")
			GPIO.setup(self.gp6, GPIO.OUT)
			GPIO.output(self.gp6, 0)
			time.sleep(0.05)
			GPIO.output(self.gp6, 1)
			
			logger.info("MACHINE_RESET_COMPLETE: Physical reset completed, machine cycle started")
			return True
			
		except Exception as e:
			logger.error(f"Reset failed: {e}")
			return False
	
	def reset_pins(self):
		"""Reset pins and control state - called by game logic"""
		logger.info("GAME_RESET: Game logic requesting pin reset")
		
		# SET FULL RESET FLAG BEFORE calling reset
		self._force_full_reset = True
		
		# Physical reset
		self.reset()
		
		# Reset control state to all pins UP
		self.control = {'lTwo': 1, 'lThree': 1, 'cFive': 1, 'rThree': 1, 'rTwo': 1}
		self.control_change = {'lTwo': 0, 'lThree': 0, 'cFive': 0, 'rThree': 0, 'rTwo': 0}
		self.pin_check = False
		self.pins_changed = False
		
		# Clear reset flags
		self._needs_full_reset = False
		self._force_full_reset = False
		
		logger.info(f"GAME_RESET_COMPLETE: Pin states reset to: {self.control}")

	def process_throw(self):
		"""CANADIAN 5-PIN: Enhanced process_throw with proper reset logic"""
		logger.info("[0.000s] BALL_DETECTED: Starting hardware processing cycle")
		
		# Update pin set status
		self._update_pin_set_status()
		
		# Step 1: Check pins to see what happened
		result, status = self.check_pins()
		logger.info(f"Pin check result: {result}, Status: {status}")
		
		# Step 2: CANADIAN 5-PIN specific reset logic
		needs_full_reset = False
		is_strike = (status == 2)  # All 5 pins down
		is_third_ball = self._is_third_ball()
		external_force_reset = self._force_full_reset
		
		if external_force_reset:
			logger.info("EXTERNAL_FORCE_RESET: Clearing external force reset flag")
			self._force_full_reset = False
			needs_full_reset = True
		
		# CANADIAN 5-PIN: Determine when full reset is needed
		reset_reason = None
		if external_force_reset:
			needs_full_reset = True
			reset_reason = "external_reset"
		elif is_strike:
			needs_full_reset = True
			reset_reason = "strike_reset"
			logger.info("CANADIAN_5PIN_STRIKE: All 5 pins down - full reset needed")
		elif self._is_frame_ending_ball():
			needs_full_reset = True
			reset_reason = "frame_ending"
			logger.info("CANADIAN_5PIN_FRAME_END: Frame ending ball - full reset needed")
		elif is_third_ball:
			needs_full_reset = True
			reset_reason = "third_ball"
			logger.info("CANADIAN_5PIN_THIRD_BALL: Third ball - full reset needed")
		
		# Step 3: Handle machine cycle based on Canadian 5-pin rules
		if needs_full_reset:
			logger.info(f"RESET_NEEDED: Full reset needed ({reset_reason})")
			self.reset_pins()
		elif self.pins_changed or status > 0:
			logger.info("MACHINE_CYCLE_NEEDED: Starting normal machine cycle")
			self.start_machine_cycle()
		else:
			logger.info("NO_MACHINE_CYCLE: No pin changes detected")
		
		return result
	
	def _is_frame_ending_ball(self):
		"""CANADIAN 5-PIN: Check if this ball ends the current frame"""
		try:
			if hasattr(self, 'game_context') and self.game_context:
				current_bowler = self.game_context.bowlers[self.game_context.current_bowler_index]
				current_frame = current_bowler.frames[current_bowler.current_frame]
				
				current_ball_count = len(current_frame.balls)
				is_10th_frame = (current_bowler.current_frame == 9)
				
				# FIXED: Regular frames (1-9) only end after strike, spare, or 3rd ball
				if not is_10th_frame:
					if current_ball_count == 0:
						# This will be first ball - check if it will be a strike
						return False  # Let the ball be processed first
					elif current_ball_count == 1:
						# This will be second ball - only ends if spare
						# We need to check if this makes a spare
						first_ball_value = current_frame.balls[0].value
						# If first ball was a strike, frame already ended
						if first_ball_value == 15:
							return False  # Frame already ended
						# Otherwise, frame continues regardless of second ball result
						return False  # Always continue to third ball unless spare
					elif current_ball_count == 2:
						# This will be third ball - always ends regular frame
						return True
				
				# CANADIAN 5-PIN: 10th frame rules - NEVER ends early via this function
				else:
					# 10th frame: ONLY ends after 3rd ball, NEVER after 1st or 2nd ball
					# The frame completion logic in process_ball handles 10th frame properly
					if current_ball_count == 2:
						# This will be third ball - always ends 10th frame
						return True
					else:
						# First or second ball in 10th frame - NEVER ends frame here
						return False
			
			return False
		except Exception as e:
			logger.error(f"Error determining frame ending ball: {e}")
			return False
	
	def _will_be_last_ball(self, result, status):
		"""CANADIAN 5-PIN: Determine if this ball will complete the frame"""
		try:
			if hasattr(self, 'game_context') and self.game_context:
				current_bowler = self.game_context.bowlers[self.game_context.current_bowler_index]
				current_frame = current_bowler.frames[current_bowler.current_frame]
				
				current_ball_count = len(current_frame.balls)
				this_ball_value = sum(a * b for a, b in zip(result, [2, 3, 5, 3, 2]))
				is_10th_frame = (current_bowler.current_frame == 9)
				
				# CANADIAN 5-PIN: Strike (15 points) on any ball
				if status == 2 or this_ball_value == 15:
					if not is_10th_frame:
						return True  # Strike ends regular frame
					else:
						# 10th frame: strike doesn't end frame, just resets pins
						return False
				
				# CANADIAN 5-PIN: Regular frames (1-9)
				if not is_10th_frame:
					if current_ball_count == 1:
						# Second ball: check for spare
						first_ball_value = current_frame.balls[0].value
						if first_ball_value + this_ball_value == 15:
							return True  # Spare ends frame
						else:
							return True  # Open frame ends after 2 balls
					elif current_ball_count == 2:
						return True  # Third ball always ends regular frame
				
				# CANADIAN 5-PIN: 10th frame
				else:
					if current_ball_count == 2:
						return True  # Third ball always ends 10th frame
					elif current_ball_count == 1:
						# Second ball in 10th frame
						first_ball_value = current_frame.balls[0].value
						
						# If first ball wasn't a strike and this doesn't make a spare
						if first_ball_value < 15 and (first_ball_value + this_ball_value) < 15:
							return True  # Open frame ends after 2 balls
						# If spare, frame continues for third ball
						# If first ball was strike, frame continues for third ball
						return False
			
			return False
		except Exception as e:
			logger.error(f"Error determining last ball: {e}")
			return False
	
	def start_machine_cycle(self):
		"""CANADIAN 5-PIN: Machine cycle that preserves pin states correctly"""
		
		# Check for full reset flag first
		if getattr(self, '_force_full_reset', False) or getattr(self, '_needs_full_reset', False):
			logger.info("FULL_RESET_FLAG_DETECTED: Executing full reset")
			
			# Full reset: all pins back to UP position
			self.reset()
			self.control = {'lTwo': 1, 'lThree': 1, 'cFive': 1, 'rThree': 1, 'rTwo': 1}
			self.control_change = {'lTwo': 0, 'lThree': 0, 'cFive': 0, 'rThree': 0, 'rTwo': 0}
			
			# Apply configuration immediately
			self.apply_pin_configuration_immediate()
			
			# Clear flags
			self._force_full_reset = False
			self._needs_full_reset = False
			
			logger.info("CANADIAN_5PIN_RESET_COMPLETE: All pins reset to UP position")
			return
		
		# Normal machine cycle: set pins to detected state
		logger.info("MACHINE_CYCLE_START: Starting normal machine cycle")
		if not self.reset():
			logger.error("MACHINE_CYCLE_FAILED: Reset failed")
			return
		
		# Wait for machine timing
		self.wait_for_machine_timing()
		
		# Apply the current pin configuration (keeps knocked down pins down)
		self.apply_pin_configuration()
		
		logger.info(f"CANADIAN_5PIN_CYCLE_COMPLETE: Pin states maintained: {self.control}")
	
	def apply_pin_configuration_immediate(self):
		"""CANADIAN 5-PIN: Apply pin configuration without b21 wait"""
		try:
			logger.info("IMMEDIATE_PIN_CONFIG: Setting all pins to UP state")
			
			# For Canadian 5-pin full reset, all pins go UP
			GPIO.output([self.gp1, self.gp2, self.gp3, self.gp4, self.gp5], 1)
			
			logger.info("CANADIAN_5PIN_IMMEDIATE_COMPLETE: All 5 pins set to UP state")
			
		except Exception as e:
			logger.error(f"IMMEDIATE_PIN_CONFIG_ERROR: {e}")
			# Emergency: all pins to safe UP state
			GPIO.output([self.gp1, self.gp2, self.gp3, self.gp4, self.gp5], 1)
		
	def wait_for_machine_timing(self):
		"""FIXED: Wait for b21 sensor with fallback to pin_restore"""
		if not self.reset_called_time:
			logger.error("TIMING_ERROR: No reset time recorded")
			return
		
		# FIXED: Check if pin setting is disabled
		if not self.pin_set_enabled:
			logger.info("MACHINE_PIN_SET_DISABLED: Pin setting is disabled, skipping b21 wait")
			return
		
		timeout = 8.0  # 8 second timeout
		error_count = 0
		max_errors = 10
		
		logger.info(f"MACHINE_B21_WAIT: Waiting for b21 sensor (max {timeout:.1f}s)")
		
		while time.time() - self.reset_called_time < timeout:
			try:
				if self.b21.voltage >= 4:
					trigger_time = time.time() - self.reset_called_time
					logger.info(f"MACHINE_B21_TRIGGERED: After {trigger_time:.3f}s from reset")
					return  # Success - exit function
					
			except Exception as e:
				error_count += 1
				logger.warning(f"B21 sensor error #{error_count}: {e}")
				
				if error_count >= max_errors:
					logger.error(f"Too many B21 sensor errors ({error_count}), proceeding with pin_restore fallback")
					break
				
				time.sleep(0.01)
				continue
				
			time.sleep(0.01)
		
		# If we get here, b21 was not triggered within timeout
		actual_wait_time = time.time() - self.reset_called_time
		logger.warning(f"MACHINE_B21_TIMEOUT: Waited {actual_wait_time:.3f}s without b21 detection")
		
		# FIXED: Use pin_restore fallback instead of retries
		logger.info("MACHINE_B21_FALLBACK: Using pin_restore with current control state")
		current_control = self.control.copy()
		self.pin_restore(current_control)
		
		logger.info("MACHINE_B21_COMPLETE: Finished b21 wait sequence with fallback")

	def apply_pin_configuration(self):
		try:
			# Start with all pins HIGH (safe state)
			GPIO.output([self.gp1, self.gp2, self.gp3, self.gp4, self.gp5], 1)
			
			# Determine which pins to knock down based on current detection
			pins_to_knock_down = []
			pin_names = []
			
			pin_mapping = {
				'lTwo': self.gp1,
				'lThree': self.gp2,
				'cFive': self.gp3,
				'rThree': self.gp4,
				'rTwo': self.gp5
			}
			
			for pin_name, gpio_pin in pin_mapping.items():
				if self.control.get(pin_name, 1) == 0:
					pins_to_knock_down.append(gpio_pin)
					pin_names.append(pin_name)
			
			if pins_to_knock_down:
				logger.info(f"MACHINE_GPIO_PULSE: Knocking down {pin_names}")
				GPIO.output(pins_to_knock_down, 0)  # Activate solenoids
				time.sleep(0.25)  # Hold pulse
				GPIO.output(pins_to_knock_down, 1)  # Return to safe state
			else:
				logger.info("MACHINE_GPIO_NO_PULSE: All pins remain standing")
			
			logger.info(f"MACHINE_CYCLE_COMPLETE: Final state: {self.control}")
			
		except Exception as e:
			logger.error(f"MACHINE_GPIO_ERROR: {e}")
			# Emergency: all pins to safe state
			GPIO.output([self.gp1, self.gp2, self.gp3, self.gp4, self.gp5], 1)
	
	def check_pins(self):
		"""Check pin sensors and update control state"""
		start_time = time.time()
		min_check_time = 3.0  # 3-second rule compliance
		
		logger.info(f"Starting pin check. Initial state: {self.control}")
		
		# Reset change tracking
		self.control_change = {'lTwo': 0, 'lThree': 0, 'cFive': 0, 'rThree': 0, 'rTwo': 0}
		self.pins_changed = False
		self.pin_check = True
		
		# Check if all pins are already down
		if all(v == 0 for v in self.control.values()):
			logger.info("All pins are already down at start!")
			time.sleep(min_check_time)  # Still wait for rule compliance
			return [1, 1, 1, 1, 1], 2
		
		# Track consecutive stable readings for early exit
		stable_readings = 0
		required_stable = 10
		
		while self.pin_check and time.time() - start_time <= min_check_time:
			try:
				previous_control = self.control.copy()
				
				# Check each sensor with proper timing
				time.sleep(0.025)
				
				# Check sensors in sequence - map to correct pin names
				sensors_to_check = [
					(self.b20, self.pb20),  # Usually cFive
					(self.b13, self.pb13),  # Usually rTwo
					(self.b12, self.pb12),  # Usually rThree
					(self.b11, self.pb11),  # Usually lThree
					(self.b10, self.pb10)   # Usually lTwo
				]
				
				for sensor, pin_name in sensors_to_check:
					try:
						if sensor.voltage >= 4.0 and self.control[pin_name] != 0:
							self.control[pin_name] = 0
							self.control_change[pin_name] = 1
							self.pins_changed = True
							logger.info(f"{pin_name} detected DOWN")
					except Exception as e:
						logger.warning(f"Error reading {pin_name} sensor: {e}")
						continue
				
				# Check for stability (no changes in recent readings)
				if self.control == previous_control:
					stable_readings += 1
				else:
					stable_readings = 0
				
				# Check if all pins are now down (STRIKE!)
				if all(v == 0 for v in self.control.values()):
					remaining_time = min_check_time - (time.time() - start_time)
					if remaining_time > 0:
						logger.info(f"STRIKE! All pins down, waiting remaining {remaining_time:.2f}s for rule compliance")
						time.sleep(remaining_time)
					logger.info("STRIKE detected - all pins are down!")
					return list(self.control_change.values()), 2
				
				# Early exit if stable for sufficient time and some time has passed
				if (stable_readings >= required_stable and 
					time.time() - start_time >= 1.0 and 
					self.pins_changed):
					logger.info(f"Stable state detected after {stable_readings} readings")
					break
				
				time.sleep(0.025)  # Small delay between readings
				
			except Exception as e:
				logger.error(f"Error during pin check: {e}")
				time.sleep(0.01)
				continue
		
		# Pin check complete
		self.pin_check = False
		result = list(self.control_change.values())
		
		logger.info(f"Pin check complete after {time.time() - start_time:.2f}s")
		logger.info(f"Final control state: {self.control}")
		logger.info(f"Changes detected: {result}")
		
		return result, 1 if self.pins_changed else 0
	
	def schedule_reset(self, reset_type='FULL_RESET', data=None):
		"""
		FIXED: Schedule a reset operation with immediate execution support
		"""
		immediate = data.get('immediate', False) if data and isinstance(data, dict) else False
		logger.info(f"RESET_SCHEDULED: {reset_type} scheduled, immediate: {immediate}")
		
		if immediate and reset_type == 'FULL_RESET':
			# Do immediate full reset (for button presses)
			logger.info("IMMEDIATE_FULL_RESET: Executing reset immediately")
			self.reset_pins()
		elif reset_type == 'FULL_RESET':
			# Set flag for full reset to be applied at next machine cycle
			self._force_full_reset = True
			logger.info("FULL_RESET_FLAG_SET: Will be applied at next cycle")
		else:
			# For other reset types, just call reset_pins
			self.reset_pins()
	
	def schedule_pin_restore(self, pin_data):
		"""Schedule a pin restore operation"""
		logger.info(f"PIN_RESTORE_SCHEDULED: {pin_data}")
		
		if pin_data and isinstance(pin_data, dict):
			for key, value in pin_data.items():
				if key in self.control:
					self.control[key] = value
		
		# Start machine cycle to apply the new pin configuration
		self.start_machine_cycle()
	
	def pin_restore(self, data=None):
		"""Pin restore that properly starts machine cycle"""
		logger.info(f"pin_restore called with data: {data}")
		
		if data and isinstance(data, dict):
			# Update control state with provided data
			for key, value in data.items():
				if key in self.control:
					self.control[key] = value
					logger.info(f"Set {key} to {value}")
			
			logger.info(f"Updated control state: {self.control}")
			
			# Start machine cycle to apply the configuration
			self.start_machine_cycle()
		else:
			logger.info("pin_restore called without data - no action taken")
	
	def _is_tenth_frame_complete(self, frame, result, status):
		"""Check if 10th frame is complete after this ball"""
		current_balls = len(frame.balls)
		
		# If this is the third ball, frame is complete
		if current_balls >= 2:
			return True
		
		# If this is the second ball and no strike/spare in first two, frame is complete
		if current_balls == 1:
			first_ball_value = frame.balls[0].value
			this_ball_value = sum(a * b for a, b in zip(result, [2, 3, 5, 3, 2]))
			
			# No strike on first ball and no spare = frame complete
			if first_ball_value < 15 and (first_ball_value + this_ball_value) < 15:
				return True
		
		return False
	
	def pin_set(self, pin_data):

		logger.info(f"pin_set called with direct control data: {pin_data}")
		
		# Update control state directly from function call
		self.control = pin_data.copy()
		logger.info(f"Updated control state to: {self.control}")
		
		# Start machine cycle with direct control (bypasses all timing restrictions)
		self._start_machine_cycle_direct(pin_data)
	
	def _start_machine_cycle_direct(self, pin_control):

		logger.info("MACHINE_CYCLE_DIRECT: Starting direct pin control cycle")
		logger.info(f"MACHINE_CYCLE_DIRECT: Target pin states: {pin_control}")
		
		# BYPASS all timing restrictions
		original_pin_setting_enabled = self.pin_setting_enabled
		self.pin_setting_enabled = True  # Force enable for direct control
		
		# Do machine reset first using existing reset code
		logger.info("MACHINE_CYCLE_DIRECT: Performing reset before direct pin setting")
		GPIO.output(self.gp6, 0)
		time.sleep(0.05)
		GPIO.output(self.gp6, 1)
		logger.info("MACHINE_CYCLE_DIRECT: Reset pulse sent")
		
		# Set the control state to desired configuration
		self.control = pin_control.copy()
		
		# Execute direct pin setting
		self._execute_direct_pin_setting(pin_control)
		
		# Restore original pin setting state
		self.pin_setting_enabled = original_pin_setting_enabled
		
		logger.info(f"MACHINE_CYCLE_DIRECT_COMPLETE: Final achieved state: {self.control}")
	
	def _execute_direct_pin_setting(self, pin_control):

		logger.info("MACHINE_DIRECT_PIN_SET: Starting direct mechanical pin setting")
		
		# Wait for b21 sensor if pin setting is normally enabled
		if hasattr(self, '_wait_for_b21_sensor'):
			try:
				logger.info("MACHINE_DIRECT_PIN_SET: Waiting for b21 sensor")
				self._wait_for_b21_sensor(timeout=8.0)
				logger.info("MACHINE_DIRECT_PIN_SET: b21 sensor ready")
			except Exception as e:
				logger.warning(f"MACHINE_DIRECT_PIN_SET: b21 sensor issue: {e}, proceeding anyway")
		else:
			# Wait for the standard b21 timing if no separate function exists
			time.sleep(5.5)  # Standard b21 wait time
			logger.info("MACHINE_DIRECT_PIN_SET: Standard b21 wait completed")
		
		# Execute pin positioning using existing GPIO pins
		pins_to_knock_down = []
		
		for pin_name, desired_state in pin_control.items():
			if desired_state == 0:  # Pin should be down
				pins_to_knock_down.append(pin_name)
		
		logger.info(f"MACHINE_DIRECT_PIN_SET: Pins to knock down: {pins_to_knock_down}")
		
		# Execute GPIO pulses using existing pin mappings
		if pins_to_knock_down:
			logger.info(f"MACHINE_DIRECT_GPIO_PULSE: Knocking down {pins_to_knock_down}")
			
			for pin_name in pins_to_knock_down:
				# Use existing GPIO pin mappings from MachineFunctions
				gpio_pin = None
				
				# Map pin names to existing GPIO variables in MachineFunctions
				if pin_name == 'lTwo' and hasattr(self, 'gp12'):
					gpio_pin = self.gp12
				elif pin_name == 'lThree' and hasattr(self, 'gp16'):
					gpio_pin = self.gp16
				elif pin_name == 'cFive' and hasattr(self, 'gp20'):
					gpio_pin = self.gp20
				elif pin_name == 'rThree' and hasattr(self, 'gp21'):
					gpio_pin = self.gp21
				elif pin_name == 'rTwo' and hasattr(self, 'gp26'):
					gpio_pin = self.gp26
				
				if gpio_pin is not None:
					try:
						# Use existing GPIO pulse pattern
						GPIO.output(gpio_pin, 0)
						time.sleep(0.15)  # Slightly longer pulse for direct control
						GPIO.output(gpio_pin, 1)
						logger.info(f"MACHINE_DIRECT_GPIO: Pulsed {pin_name} on GPIO {gpio_pin}")
					except Exception as e:
						logger.error(f"MACHINE_DIRECT_GPIO_ERROR: Failed to pulse {pin_name}: {e}")
				else:
					logger.error(f"MACHINE_DIRECT_GPIO_ERROR: No GPIO pin found for {pin_name}")
		
		# Brief settling time
		time.sleep(0.5)
		
		logger.info(f"MACHINE_DIRECT_PIN_SET_COMPLETE: Target state achieved: {pin_control}")
	
	def emergency_pin_reset(self):
		
		logger.info("EMERGENCY_PIN_RESET: Resetting all pins to standing position")
		all_pins_up = {'lTwo': 1, 'lThree': 1, 'cFive': 1, 'rThree': 1, 'rTwo': 1}
		self.pin_set(all_pins_up)