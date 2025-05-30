#!/usr/bin/env python3
"""
Cisco Interface Compliance Script for LogZilla.

This script monitors Cisco interface status events, sends notifications to Slack,
and automatically brings down interfaces back up when they go down.

The script is an example of how to use an advanced trigger in LogZilla to monitor
Cisco interfaces and take action when they go down.

Please refer to https://docs.logzilla.net/02_Creating_Triggers/03_Trigger_Scripts/#custom-scripts
for more information.

Version: 2.0.0
Author: LogZilla Team
"""

import os
import re
import sys
import json
import time
import yaml
import socket
import logging
import ipaddress
import urllib3
from pathlib import Path
import requests
from netmiko import ConnectHandler, NetmikoTimeoutException, NetmikoAuthenticationException

# Constants
SLACK_COLOR_DANGER = "#9C1A22"  # Red color for down status
SLACK_COLOR_SUCCESS = "#008000"  # Green color for up status
SLACK_EMOJI_DOWN = "🔴"
SLACK_EMOJI_UP = "🟢"
STATUS_DOWN = "DOWN"
STATUS_UP = "RECOVERED"
DEFAULT_TIMEOUT = 10
REQUIRED_CONFIG_KEYS = ["ciscoUsername", "ciscoPassword", "posturl"]


class LoggingConfigurator:
    """Configure logging settings for the application."""
    
    @staticmethod
    def setup_logging():
        """Set up logging to stdout/stderr with level from environment variable."""
        # Get log level from environment variable or default to INFO
        log_level_name = os.environ.get('LOG_LEVEL', 'INFO')
        log_level = getattr(logging, log_level_name.upper(), logging.INFO)
        
        # Configure root logger
        logging.basicConfig(
            level=log_level,
            format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
        )
        
        # Set third-party loggers to a higher level to reduce noise
        logging.getLogger('paramiko').setLevel(logging.WARNING)
        logging.getLogger('netmiko').setLevel(logging.WARNING)
        logging.getLogger('urllib3').setLevel(logging.WARNING)
        
        # Suppress InsecureRequestWarning for Slack API calls with proper verification
        urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
        
        logging.debug(f"Log level set to: {log_level_name}")


class ConfigLoader:
    """Load and validate configuration from YAML files."""
    
    @staticmethod
    def load_config(config_file):
        """
        Load configuration from a YAML file.
        
        Args:
            config_file (str): Path to the YAML configuration file.
            
        Returns:
            dict: Configuration as a dictionary.
            
        Raises:
            SystemExit: If the file is not found, contains invalid YAML,
                or is missing required configuration keys.
        """
        try:
            with open(config_file, 'r') as file:
                config = yaml.safe_load(file)
                
            # Validate required configuration keys
            missing_keys = [key for key in REQUIRED_CONFIG_KEYS if key not in config]
            if missing_keys:
                logging.error(f"Missing required configuration keys: {', '.join(missing_keys)}")
                sys.exit(1)
                
            return config
        except FileNotFoundError:
            logging.error(f"Configuration file {config_file} not found.")
            sys.exit(1)
        except yaml.YAMLError as e:
            logging.error(f"Error parsing configuration file: {e}")
            sys.exit(1)

class NetworkUtils:
    """Network-related utility functions."""
    
    @staticmethod
    def is_valid_ip(ip_string):
        """
        Check if a string is a valid IP address.
        
        Args:
            ip_string (str): The string to check.
            
        Returns:
            bool: True if the string is a valid IP address, False otherwise.
        """
        try:
            ipaddress.ip_address(ip_string)
            return True
        except ValueError:
            return False
    
    @staticmethod
    def resolve_host(hostname, config):
        """
        Resolve hostname to IP address.
        
        Args:
            hostname (str): Hostname to resolve.
            config (dict): Configuration dictionary that may contain a fallback IP.
            
        Returns:
            str: IP address if resolution is successful.
            
        Raises:
            SystemExit: If resolution fails and no fallback IP is available.
        """
        # Check if hostname is already an IP address
        if NetworkUtils.is_valid_ip(hostname):
            logging.debug(f"EVENT_HOST is a valid IP address: {hostname}")
            return hostname
        
        # Try to resolve the hostname to an IP address
        try:
            ip_address = socket.gethostbyname(hostname)
            logging.debug(f"Resolved EVENT_HOST to IP: {ip_address}")
            return ip_address
        except socket.gaierror:
            logging.error(f"Unable to resolve hostname: {hostname}")
            
            # Try to get the IP address from the config file
            ip_address = config.get('EVENT_HOST_IP', '')
            if not ip_address:
                logging.error("No valid IP address found for EVENT_HOST")
                sys.exit(1)
            
            logging.debug(f"Using IP address from config: {ip_address}")
            return ip_address

class SlackNotifier:
    """Send notifications to Slack."""
    
    def __init__(self, webhook_url, timeout=DEFAULT_TIMEOUT):
        """
        Initialize with Slack webhook URL.
        
        Args:
            webhook_url (str): Slack webhook URL.
            timeout (int): Request timeout in seconds.
        """
        self.webhook_url = webhook_url
        self.timeout = timeout
    
    def send_interface_notification(self, event_host, interface, state, 
                                   description, event_message, status, 
                                   emoji, color, mnemonic):
        """
        Send interface status notification to Slack.
        
        Args:
            event_host (str): Device hostname.
            interface (str): Interface name.
            state (str): Interface state (up/down).
            description (str): Interface description.
            event_message (str): Original event message.
            status (str): Status text (e.g., "DOWN", "RECOVERED").
            emoji (str): Status emoji.
            color (str): Message color for Slack.
            mnemonic (str): Cisco mnemonic code.
            
        Returns:
            bool: True if notification was sent successfully, False otherwise.
        """
        timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
        
        payload = {
            "text": f"{emoji} {status}: Interface {interface} on {event_host}",
            "attachments": [
                {
                    "color": color,
                    "blocks": [
                        {
                            "type": "section",
                            "fields": [
                                {"type": "mrkdwn", "text": f"*Device:*\n{event_host}"},
                                {"type": "mrkdwn", "text": f"*Interface:*\n{interface}"},
                                {"type": "mrkdwn", "text": f"*State:*\n{state}"},
                                {"type": "mrkdwn", "text": f"*Description:*\n{description}"},
                                {
                                    "type": "mrkdwn", 
                                    "text": f"*Program:*\n{os.environ.get('EVENT_PROGRAM', 'Unknown')}"
                                },
                                {
                                    "type": "mrkdwn", 
                                    "text": f"*Severity:*\n{os.environ.get('EVENT_SEVERITY', 'Unknown')}"
                                },
                            ]
                        },
                        {
                            "type": "section",
                            "text": {
                                "type": "mrkdwn",
                                "text": f"*Event Message:*\n```{event_message}```"
                            }
                        },
                        {
                            "type": "context",
                            "elements": [
                                {
                                    "type": "mrkdwn",
                                    "text": (
                                        f"🔗 <https://search.cisco.com/search?"
                                        f"query=%25{mnemonic}:&locale=enUS&cat="
                                        f"&mode=text&clktyp=enter&autosuggest=false|{mnemonic}>"
                                    )
                                },
                                {
                                    "type": "mrkdwn",
                                    "text": f"🕒 {timestamp}"
                                }
                            ]
                        }
                    ]
                }
            ]
        }
        
        return self._send_payload(payload)
    
    def send_error_notification(self, event_host, error_message):
        """
        Send error notification to Slack.
        
        Args:
            event_host (str): Device hostname.
            error_message (str): Error message.
            
        Returns:
            bool: True if notification was sent successfully, False otherwise.
        """
        timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
        
        error_payload = {
            "text": f"🔴 ERROR: Script execution failed for {event_host}",
            "attachments": [
                {
                    "color": "#9C1A22",
                    "blocks": [
                        {
                            "type": "section",
                            "text": {
                                "type": "mrkdwn",
                                "text": f"*Error Message:*\n```{error_message}```"
                            }
                        },
                        {
                            "type": "context",
                            "elements": [
                                {
                                    "type": "mrkdwn",
                                    "text": f"🕒 {timestamp}"
                                }
                            ]
                        }
                    ]
                }
            ]
        }
        
        return self._send_payload(error_payload)
    
    def _send_payload(self, payload):
        """
        Send payload to Slack.
        
        Args:
            payload (dict): Message payload.
            
        Returns:
            bool: True if successful, False otherwise.
        """
        try:
            response = requests.post(
                self.webhook_url, 
                json=payload, 
                timeout=self.timeout, 
                verify=True
            )
            
            if response.status_code != 200:
                logging.error(
                    f"Request to Slack returned an error {response.status_code}, "
                    f"the response is:\n{response.text}"
                )
                return False
            
            logging.info("Successfully posted to Slack")
            logging.debug(json.dumps(payload))
            return True
            
        except Exception as e:
            logging.error(f"Error sending notification to Slack: {str(e)}")
            return False

class CiscoDeviceManager:
    """Manage Cisco device connections and operations."""
    
    def __init__(self, username, password, timeout=DEFAULT_TIMEOUT):
        """
        Initialize with device credentials.
        
        Args:
            username (str): Cisco device username.
            password (str): Cisco device password.
            timeout (int): Connection timeout in seconds.
        """
        self.username = username
        self.password = password
        self.timeout = timeout
        self.device = None
    
    def connect(self, host):
        """
        Connect to a Cisco device.
        
        Args:
            host (str): Device IP address or hostname.
            
        Returns:
            bool: True if connection is successful, False otherwise.
            
        Raises:
            NetmikoTimeoutException: If connection times out.
            NetmikoAuthenticationException: If authentication fails.
        """
        logging.info(f"Attempting to connect to: {host}")
        
        device_params = {
            'device_type': 'cisco_ios',
            'host': host,
            'username': self.username,
            'password': self.password,
            'timeout': self.timeout
        }
        
        self.device = ConnectHandler(**device_params)
        return True
    
    def disconnect(self):
        """Safely disconnect from the device if connected."""
        if self.device and self.device.is_alive():
            self.device.disconnect()
    
    def configure_interface(self, interface, command):
        """
        Configure an interface on the connected device.
        
        Args:
            interface (str): Interface name.
            command (str): Command to apply to the interface.
            
        Returns:
            bool: True if configuration was successful, False otherwise.
        """
        try:
            config_commands = [
                f"interface {interface}",
                command,
                "exit"
            ]
            output = self.device.send_config_set(config_commands)
            logging.info(f"Configuration output:\n{output}")
            return True
        except Exception as e:
            logging.error(f"Failed to configure interface: {str(e)}")
            return False
    
    def get_interface_description(self, interface):
        """
        Get the description of an interface.
        
        Args:
            interface (str): Interface name.
            
        Returns:
            str: Interface description or "No description found".
        """
        output = self.device.send_command(f"show interface {interface} | include Description")
        return self._extract_interface_description(output)
    
    @staticmethod
    def _extract_interface_description(output):
        """
        Extract description from command output.
        
        Args:
            output (str): Command output from 'show interface | inc Desc'.
            
        Returns:
            str: Interface description or "No description found".
        """
        lines = output.splitlines()
        for line in lines:
            line = line.strip()
            logging.debug(f"Processing line: {line}")
            if "Description:" in line:
                description = line.split("Description:", 1)[1].strip()
                logging.debug(f"Extracted description: {description}")
                return description
        return "No description found"
    
    @staticmethod
    def parse_interface_event(event_message):
        """
        Parse interface and state from an event message.
        
        Args:
            event_message (str): Event message to parse.
            
        Returns:
            tuple: (interface_name, state) or (None, None) if parsing fails.
        """
        if not event_message:
            return None, None
        
        logging.debug(f"Parsing event message: {event_message}")
        
        # Check if the interface is reported as down
        match_down = re.search(r"Interface (\S+), changed state to down", event_message)
        if match_down:
            return match_down.group(1), "down"
        
        # Check if the interface is reported as up
        match_up = re.search(r"Interface (\S+), changed state to up", event_message)
        if match_up:
            return match_up.group(1), "up"
        
        return None, None

class ComplianceApplication:
    """Main application class for Cisco interface compliance."""
    
    def __init__(self):
        """Initialize the application."""
        # Set up logging
        LoggingConfigurator.setup_logging()
        
        # Optionally print environment variables for debugging
        if logging.getLogger().getEffectiveLevel() <= logging.DEBUG:
            self._print_environment_variables()
        
        # Load configuration
        self.config = ConfigLoader.load_config('/scripts/compliance.yaml')
        
        # Initialize components
        self.slack = SlackNotifier(
            webhook_url=self.config['posturl'],
            timeout=self.config.get('timeout', DEFAULT_TIMEOUT)
        )
        
        self.cisco_manager = CiscoDeviceManager(
            username=self.config['ciscoUsername'],
            password=self.config['ciscoPassword'],
            timeout=self.config.get('timeout', DEFAULT_TIMEOUT)
        )
    
    def _print_environment_variables(self):
        """Print all EVENT_ environment variables if debugging is enabled."""
        logging.debug("Incoming Event Variables:")
        for key, value in os.environ.items():
            if key.startswith("EVENT"):
                logging.debug(f"{key} = {value}")
    
    def run(self):
        """Run the main application logic."""
        try:
            # Get and resolve hostname
            event_host = os.environ.get('EVENT_HOST', self.config.get('EVENT_HOST', ''))
            logging.debug(f"Original EVENT_HOST: {event_host}")
            
            # Resolve hostname to IP
            event_host_ip = NetworkUtils.resolve_host(event_host, self.config)
            
            # Connect to the device
            self.cisco_manager.connect(event_host_ip)
            
            # Process the event message
            event_message = os.environ.get('EVENT_MESSAGE', '')
            interface, state = self.cisco_manager.parse_interface_event(event_message)
            
            if not interface:
                logging.error("Unable to obtain interface name from event message")
                sys.exit(1)
            
            logging.debug(f"Detected interface: {interface}, state: {state}")
            
            # Get interface description
            description = self.cisco_manager.get_interface_description(interface)
            logging.debug(f"Interface Description: {description}")
            
            # Handle interface state
            self._handle_interface_state(
                event_host, 
                interface, 
                state, 
                description, 
                event_message
            )
            
        except (NetmikoTimeoutException, NetmikoAuthenticationException) as e:
            logging.error(f"Netmiko error occurred: {str(e)}")
            logging.exception("Exception details:")
            
            # Send error notification
            self.slack.send_error_notification(event_host, str(e))
            
        finally:
            # Ensure we disconnect cleanly
            self.cisco_manager.disconnect()
    
    def _handle_interface_state(self, event_host, interface, state, description, event_message):
        """
        Handle interface state changes.
        
        Args:
            event_host (str): Device hostname.
            interface (str): Interface name.
            state (str): Interface state ("up" or "down").
            description (str): Interface description.
            event_message (str): Original event message.
        """
        mnemonic = os.environ.get('EVENT_CISCO_MNEMONIC', '')
        
        if state == "down":
            # Send notification that the interface is down
            self.slack.send_interface_notification(
                event_host, interface, "down", description, event_message,
                STATUS_DOWN, SLACK_EMOJI_DOWN, SLACK_COLOR_DANGER, mnemonic
            )
            
            # Bring the interface back up if configured to do so
            if self.config.get('bring_interface_up', True):
                logging.info(f"Action: Bringing interface {interface} back up")
                if self.cisco_manager.configure_interface(interface, "no shutdown"):
                    logging.info(f"Success: Interface {interface} should be coming back up")
                
        elif state == "up":
            # Send notification that the interface is up
            self.slack.send_interface_notification(
                event_host, interface, "up", description, event_message,
                STATUS_UP, SLACK_EMOJI_UP, SLACK_COLOR_SUCCESS, mnemonic
            )


def main():
    """Main entry point for the script."""
    app = ComplianceApplication()
    app.run()


if __name__ == "__main__":
    main()

