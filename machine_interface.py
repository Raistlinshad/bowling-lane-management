#!/usr/bin/env python3
"""
Machine Interface Bridge
Connects C++ Qt client to MachineFunctions.py ball detector
Outputs JSON messages to stdout for C++ to read
"""

import sys
import json
import time
import threading
import logging
from queue import Queue, Empty
import signal
import os
from logging.handlers import RotatingFileHandler

def setup_logging(log_file_path='log.txt', max_log_size=10*1024*1024, backup_count=5):
	# Create formatter for regular log messages
	log_formatter = logging.Formatter(
		'%(asctime)s - %(levelname)s - %(message)s', 
		datefmt='%d/%m/%H:%M'
	)
	 
	# Create console handler and set level
	console_handler = logging.StreamHandler()
	console_handler.setFormatter(log_formatter)
	
	# Create file handler with rotation to manage file size
	file_handler = RotatingFileHandler(
		log_file_path, 
		maxBytes=max_log_size,
		backupCount=backup_count
	)
	file_handler.setFormatter(log_formatter)
	
	# Get the root logger and set its level
	root_logger = logging.getLogger()
	root_logger.setLevel(logging.INFO)
	
	# Remove any existing handlers
	for handler in root_logger.handlers[:]:
		root_logger.removeHandler(handler)
	
	# Add our handlers
	root_logger.addHandler(console_handler)
	root_logger.addHandler(file_handler)
	
	# Add special startup entry with date and entry number
	current_date = datetime.now().strftime('%d/%m/%Y')
	
	# Calculate entry number by counting existing entries in log file
	entry_number = 1
	if os.path.exists(log_file_path):
		with open(log_file_path, 'r') as f:
			for line in f:
				if f"Log {current_date.split('/')[0]}/{current_date.split('/')[1]}" in line and "entry #" in line:
					entry_number += 1
	
	# Log startup message
	root_logger.info(f"Log {current_date} entry #{entry_number}")
	
	return root_logger

logger = setup_logging()


# Import your existing machine functions
try:
    from MachineFunctions import MachineFunctions
    from active_ball_detector import ActiveBallDetector
except ImportError as e:
    print(json.dumps({"type": "error", "message": f"Failed to import machine modules: {e}"}))
    sys.exit(1)

class MachineInterfaceBridge:
    def __init__(self):
        self.running = True
        self.machine = None
        self.ball_detector = None
        self.command_queue = Queue()
        self.output_queue = Queue()
        
        # Setup logging to file (not stdout to avoid interfering with JSON output)
        logging.basicConfig(
            filename='machine_interface.log',
            level=logging.INFO,
            format='%(asctime)s - %(levelname)s - %(message)s'
        )
        self.logger = logging.getLogger(__name__)
        
        # Signal handlers for clean shutdown
        signal.signal(signal.SIGTERM, self.signal_handler)
        signal.signal(signal.SIGINT, self.signal_handler)
        
    def signal_handler(self, signum, frame):
        self.logger.info(f"Received signal {signum}, shutting down")
        self.shutdown()
        
    def initialize_machine(self):
        """Initialize machine functions"""
        try:
            self.machine = MachineFunctions()
            self.send_output({"type": "machine_ready", "message": "Machine functions initialized"})
            self.logger.info("Machine functions initialized successfully")
            return True
        except Exception as e:
            error_msg = f"Failed to initialize machine: {e}"
            self.send_output({"type": "error", "message": error_msg})
            self.logger.error(error_msg)
            return False
    
    def start_ball_detection(self):
        """Start the active ball detector"""
        try:
            if not self.machine:
                raise Exception("Machine not initialized")
            
            # Create a simple game-like object for the ball detector
            class SimpleGame:
                def __init__(self, bridge):
                    self.bridge = bridge
                    
                def process_ball(self, result):
                    """Called by ball detector when ball is detected"""
                    self.bridge.on_ball_detected(result)
            
            simple_game = SimpleGame(self)
            
            # Start ball detector
            self.ball_detector = ActiveBallDetector(simple_game, self.machine)
            
            self.send_output({"type": "detection_started", "message": "Ball detection active"})
            self.logger.info("Ball detection started")
            return True
            
        except Exception as e:
            error_msg = f"Failed to start ball detection: {e}"
            self.send_output({"type": "error", "message": error_msg})
            self.logger.error(error_msg)
            return False
    
    def stop_ball_detection(self):
        """Stop ball detection"""
        try:
            if self.ball_detector:
                self.ball_detector.running = False
                self.ball_detector = None
                
            self.send_output({"type": "detection_stopped", "message": "Ball detection stopped"})
            self.logger.info("Ball detection stopped")
            
        except Exception as e:
            error_msg = f"Error stopping ball detection: {e}"
            self.send_output({"type": "error", "message": error_msg})
            self.logger.error(error_msg)
    
    def on_ball_detected(self, result):
        """Called when ball is detected by active_ball_detector"""
        try:
            # result should be [0,0,0,0,0] format where 1=pin down, 0=pin up
            # Convert to our format where 1=pin up, 0=pin down for consistency with Qt
            pins = [1 - x for x in result]  # Invert: 0->1, 1->0
            
            # Calculate Canadian 5-pin value
            pin_values = [2, 3, 5, 3, 2]  # lTwo, lThree, cFive, rThree, rTwo
            total_value = sum(pin_values[i] for i in range(5) if result[i] == 1)
            
            output = {
                "type": "ball_detected",
                "pins": pins,  # For Qt display (1=up, 0=down)
                "raw_result": result,  # Original machine result
                "value": total_value,
                "timestamp": time.time()
            }
            
            self.send_output(output)
            self.logger.info(f"Ball detected: {result} -> value {total_value}")
            
        except Exception as e:
            error_msg = f"Error processing ball detection: {e}"
            self.send_output({"type": "error", "message": error_msg})
            self.logger.error(error_msg)
    
    def process_command(self, command):
        """Process command from C++ client"""
        try:
            cmd_type = command.get("type", "")
            data = command.get("data", {})
            
            if cmd_type == "start_detection":
                self.start_ball_detection()
                
            elif cmd_type == "stop_detection":
                self.stop_ball_detection()
                
            elif cmd_type == "machine_reset":
                self.machine_reset(data)
                
            elif cmd_type == "pin_set":
                self.pin_set(data)
                
            elif cmd_type == "pin_restore":
                self.pin_restore(data)
                
            elif cmd_type == "hold":
                self.set_hold(data.get("held", False))
                
            elif cmd_type == "status":
                self.send_status()
                
            elif cmd_type == "ping":
                self.send_output({"type": "pong", "timestamp": time.time()})
                
            else:
                self.send_output({"type": "error", "message": f"Unknown command: {cmd_type}"})
                
        except Exception as e:
            error_msg = f"Error processing command {command}: {e}"
            self.send_output({"type": "error", "message": error_msg})
            self.logger.error(error_msg)
    
    def machine_reset(self, data):
        """Reset machine pins"""
        try:
            if not self.machine:
                raise Exception("Machine not initialized")
                
            immediate = data.get("immediate", False)
            reset_type = data.get("reset_type", "FULL_RESET")
            
            self.machine.schedule_reset(reset_type, {"immediate": immediate})
            
            self.send_output({
                "type": "machine_reset_complete",
                "reset_type": reset_type,
                "immediate": immediate
            })
            
        except Exception as e:
            self.send_output({"type": "error", "message": f"Machine reset failed: {e}"})
    
    def pin_set(self, data):
        """Set specific pin configuration"""
        try:
            if not self.machine:
                raise Exception("Machine not initialized")
                
            pin_config = data.get("pins", {})
            # Convert from Qt format (1=up, 0=down) to machine format
            machine_config = {}
            
            for pin_name, state in pin_config.items():
                machine_config[pin_name] = state
                
            self.machine.pin_set(machine_config)
            
            self.send_output({
                "type": "pin_set_complete",
                "configuration": pin_config
            })
            
        except Exception as e:
            self.send_output({"type": "error", "message": f"Pin set failed: {e}"})
    
    def pin_restore(self, data):
        """Restore pins to specific state"""
        try:
            if not self.machine:
                raise Exception("Machine not initialized")
                
            pin_config = data.get("pins", {})
            self.machine.pin_restore(pin_config)
            
            self.send_output({
                "type": "pin_restore_complete",
                "configuration": pin_config
            })
            
        except Exception as e:
            self.send_output({"type": "error", "message": f"Pin restore failed: {e}"})
    
    def set_hold(self, held):
        """Set ball detector hold state"""
        try:
            if self.ball_detector:
                self.ball_detector.set_suspended(held)
                
            self.send_output({
                "type": "hold_set",
                "held": held
            })
            
        except Exception as e:
            self.send_output({"type": "error", "message": f"Set hold failed: {e}"})
    
    def send_status(self):
        """Send current machine status"""
        try:
            status = {
                "type": "status",
                "machine_initialized": self.machine is not None,
                "detection_active": self.ball_detector is not None,
                "detection_suspended": self.ball_detector.suspended if self.ball_detector else False,
                "timestamp": time.time()
            }
            
            self.send_output(status)
            
        except Exception as e:
            self.send_output({"type": "error", "message": f"Status request failed: {e}"})
    
    def send_output(self, data):
        """Send JSON output to stdout for C++ to read"""
        try:
            json_str = json.dumps(data, separators=(',', ':'))
            print(json_str, flush=True)
        except Exception as e:
            # Last resort error output
            error_obj = {"type": "error", "message": f"JSON output failed: {e}"}
            print(json.dumps(error_obj), flush=True)
    
    def input_reader_thread(self):
        """Thread to read commands from stdin"""
        try:
            while self.running:
                try:
                    line = sys.stdin.readline()
                    if not line:
                        break
                        
                    line = line.strip()
                    if not line:
                        continue
                        
                    try:
                        command = json.loads(line)
                        self.command_queue.put(command)
                    except json.JSONDecodeError as e:
                        self.send_output({"type": "error", "message": f"Invalid JSON: {e}"})
                        
                except Exception as e:
                    self.logger.error(f"Input reader error: {e}")
                    break
                    
        except Exception as e:
            self.logger.error(f"Input reader thread failed: {e}")
    
    def main_loop(self):
        """Main processing loop"""
        # Start input reader thread
        input_thread = threading.Thread(target=self.input_reader_thread, daemon=True)
        input_thread.start()
        
        # Send startup message
        self.send_output({"type": "startup", "message": "Machine interface bridge started"})
        
        # Initialize machine
        if not self.initialize_machine():
            self.shutdown()
            return
        
        # Main processing loop
        heartbeat_counter = 0
        
        while self.running:
            try:
                # Process commands from queue
                try:
                    command = self.command_queue.get(timeout=1.0)
                    self.process_command(command)
                except Empty:
                    pass
                
                # Send periodic heartbeat
                heartbeat_counter += 1
                if heartbeat_counter >= 30:  # Every 30 seconds
                    self.send_output({"type": "heartbeat", "timestamp": time.time()})
                    heartbeat_counter = 0
                
                # Small delay to prevent CPU spinning
                time.sleep(0.1)
                
            except KeyboardInterrupt:
                break
            except Exception as e:
                self.logger.error(f"Main loop error: {e}")
                self.send_output({"type": "error", "message": f"Main loop error: {e}"})
                break
        
        self.shutdown()
    
    def shutdown(self):
        """Clean shutdown"""
        self.logger.info("Shutting down machine interface bridge")
        self.running = False
        
        try:
            self.stop_ball_detection()
        except:
            pass
        
        try:
            if self.machine:
                # Emergency pin reset on shutdown
                self.machine.emergency_pin_reset()
        except:
            pass
        
        self.send_output({"type": "shutdown", "message": "Machine interface bridge stopped"})

def main():
    """Main entry point"""
    try:
        bridge = MachineInterfaceBridge()
        bridge.main_loop()
    except Exception as e:
        error_output = {"type": "fatal_error", "message": f"Bridge startup failed: {e}"}
        print(json.dumps(error_output), flush=True)
        sys.exit(1)

if __name__ == "__main__":
    main()