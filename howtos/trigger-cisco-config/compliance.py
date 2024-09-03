#!/usr/bin/env python3

import os
import requests
import json
import logging
import yaml
import re
import socket
import ipaddress
import time
from pathlib import Path
from netmiko import ConnectHandler, NetmikoTimeoutException, NetmikoAuthenticationException

def print_env_vars():
    """
    Prints all environment variables that start with 'EVENT' if debugging is enabled.
    """
    logging.debug("Incoming Event Variables:")
    for key, value in os.environ.items():
        if key.startswith("EVENT"):
            logging.debug(f"{key} = {value}")

def load_config(config_file):
    try:
        with open(config_file, 'r') as file:
            return yaml.safe_load(file)
    except FileNotFoundError:
        logging.error(f"Configuration file {config_file} not found.")
        exit(1)
    except yaml.YAMLError as e:
        logging.error(f"Error parsing configuration file: {e}")
        exit(1)

def is_valid_ip(ip_string):
    try:
        ipaddress.ip_address(ip_string)
        return True
    except ValueError:
        return False

def configure_interface(device, interface, command):
    try:
        config_commands = [
            f"interface {interface}",
            command,
            "exit"
        ]
        output = device.send_config_set(config_commands)
        logging.info(f"Configuration output:\n{output}")
        return True
    except Exception as e:
        logging.error(f"Failed to configure interface: {str(e)}")
        return False

def send_slack_notification(event_host, interface, intState, intDesc, event_message, status_word, status_emoji, color, mnemonic, timestamp, posturl, timeout):
    payload = {
        "text": f"{status_emoji} {status_word}: Interface {interface} on {event_host}",
        "attachments": [
            {
                "color": color,
                "blocks": [
                    {
                        "type": "section",
                        "fields": [
                            {"type": "mrkdwn", "text": f"*Device:*\n{event_host}"},
                            {"type": "mrkdwn", "text": f"*Interface:*\n{interface}"},
                            {"type": "mrkdwn", "text": f"*State:*\n{intState}"},
                            {"type": "mrkdwn", "text": f"*Description:*\n{intDesc}"},
                            {"type": "mrkdwn", "text": f"*Program:*\n{os.environ.get('EVENT_PROGRAM', 'Unknown')}"},
                            {"type": "mrkdwn", "text": f"*Severity:*\n{os.environ.get('EVENT_SEVERITY', 'Unknown')}"},
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
                                "text": f"🔗 <https://search.cisco.com/search?query=%25{mnemonic}:&locale=enUS&cat=&mode=text&clktyp=enter&autosuggest=false|{mnemonic}>"
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

    response = requests.post(posturl, json=payload, timeout=timeout, verify=False)

    if response.status_code != 200:
        logging.error(f"Request to Slack returned an error {response.status_code}, the response is:\n{response.text}")
    else:
        logging.info("Successfully posted to Slack")

    logging.debug(json.dumps(payload))

def extract_interface_description(output):
    """
    Extracts the description from the filtered command output.
    Assumes the output is from 'show interface | inc Desc'.
    """
    lines = output.splitlines()
    for line in lines:
        line = line.strip()
        logging.debug(f"Processing line: {line}")
        # Check if the line contains 'Description:' and extract everything after it
        if "Description:" in line:
            description = line.split("Description:", 1)[1].strip()
            logging.debug(f"Extracted description: {description}")
            return description
    return "No description found"

# Set up logging
logging.basicConfig(filename='/var/log/logzilla/logzilla.log', level=logging.DEBUG, filemode='a')

# Print all environment variables if debugging is enabled
print_env_vars()

# Load configuration
config_file = '/etc/logzilla/scripts/.cisco-compliance.yaml'
config = load_config(config_file)

# Configuration settings
ciscoUsername = config['ciscoUsername']
ciscoPassword = config['ciscoPassword']
posturl = config['posturl']
default_channel = config['default_channel']
slack_user = config['slack_user']
timeout = config.get('timeout', 10)

# Get EVENT_HOST from environment or config
event_host = os.environ.get('EVENT_HOST', config.get('EVENT_HOST', ''))
logging.info(f"Original EVENT_HOST: {event_host}")

# Check if event_host is a valid IP address
if is_valid_ip(event_host):
    event_host_ip = event_host
    logging.info(f"EVENT_HOST is a valid IP address: {event_host_ip}")
else:
    # Try to resolve the hostname to an IP address
    try:
        event_host_ip = socket.gethostbyname(event_host)
        logging.info(f"Resolved EVENT_HOST to IP: {event_host_ip}")
    except socket.gaierror:
        logging.error(f"Unable to resolve hostname: {event_host}")
        # Try to get the IP address from the config file
        event_host_ip = config.get('EVENT_HOST_IP', '')
        if not event_host_ip:
            logging.error("No valid IP address found for EVENT_HOST")
            exit(1)
        logging.info(f"Using IP address from config: {event_host_ip}")

logging.info(f"Attempting to connect to: {event_host_ip}")

try:
    # Use Netmiko to connect to the device
    device_params = {
        'device_type': 'cisco_ios',
        'host': event_host_ip,
        'username': ciscoUsername,
        'password': ciscoPassword,
        'timeout': timeout
    }
    
    device = ConnectHandler(**device_params)

    # Extract the config for the offending interface
    interface, intState = None, None
    event_message = os.environ.get('EVENT_MESSAGE', '')
    if event_message:
        logging.info(f"INFO: {Path(__file__).name} - Incoming event message: {event_message}")
        
        # Check if the interface is reported as down
        match_down = re.search(r"Interface (\S+), changed state to down", event_message)
        if match_down:
            interface = match_down.group(1)
            intState = "down"

        # Check if the interface is reported as up
        match_up = re.search(r"Interface (\S+), changed state to up", event_message)
        if match_up:
            interface = match_up.group(1)
            intState = "up"

    if not interface:
        logging.error(f"ERROR: {Path(__file__).name} - unable to obtain interface name from event message")
        exit(1)

    logging.info(f"Detected interface: {interface}, state: {intState}")

    # Try to get interface description
    output = device.send_command(f"show interface {interface} | include Description")
    intDesc = extract_interface_description(output)
    logging.info(f"\n--Interface Description:\n{intDesc}\n---\n")

    # Check interface state and act accordingly
    if intState == "down":
        # Send notification that the interface is down
        send_slack_notification(event_host, interface, "down", intDesc, event_message, "DOWN", "🔴", "#9C1A22", os.environ.get('EVENT_CISCO_MNEMONIC', ''), time.strftime("%Y-%m-%d %H:%M:%S"), posturl, timeout)

        # Bring the interface back up
        logging.info(f"Bringing interface {interface} back up.")
        if configure_interface(device, interface, "no shutdown"):
            logging.info(f"Interface {interface} should be coming back up.")

    elif intState == "up":
        # Send notification that the interface is up
        send_slack_notification(event_host, interface, "up", intDesc, event_message, "RECOVERED", "🟢", "#008000", os.environ.get('EVENT_CISCO_MNEMONIC', ''), time.strftime("%Y-%m-%d %H:%M:%S"), posturl, timeout)

except (NetmikoTimeoutException, NetmikoAuthenticationException) as e:
    logging.error(f"Netmiko error occurred: {str(e)}")
    logging.exception("Exception details:")

    # Send a Slack notification about the error
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
                            "text": f"*Error Message:*\n```{str(e)}```"
                        }
                    },
                    {
                        "type": "context",
                        "elements": [
                            {
                                "type": "mrkdwn",
                                "text": f"🕒 {time.strftime('%Y-%m-%d %H:%M:%S')}"
                            }
                        ]
                    }
                ]
            }
        ]
    }
    requests.post(posturl, json=error_payload, timeout=timeout, verify=False)

finally:
    if 'device' in locals() and device.is_alive():
        device.disconnect()

