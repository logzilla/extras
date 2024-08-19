#!/usr/bin/env python3

import os
import paramiko
import requests
import json
import logging
import yaml
import re
import socket
import ipaddress
import time
from pathlib import Path

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

def execute_ssh_command(ssh, command):
    stdin, stdout, stderr = ssh.exec_command(command)
    output = stdout.read().decode().strip()
    error = stderr.read().decode().strip()
    return output, error

def is_valid_ip(ip_string):
    try:
        ipaddress.ip_address(ip_string)
        return True
    except ValueError:
        return False

# Set up logging
logging.basicConfig(filename='/var/log/logzilla/logzilla.log', level=logging.DEBUG, filemode='a')

# Load configuration
config_file = '/etc/logzilla/scripts/.compliance.yaml'
config = load_config(config_file)

# Simulated mode setup
simulated_mode = config.get('simulated_mode', False)
if simulated_mode and 'simulated' in config:
    os.environ.update(config['simulated'])
    logging.info("Running in simulated mode")
else:
    logging.info("Running in real mode")

# Configuration settings
ciscoUsername = config['ciscoUsername']
ciscoPassword = config['ciscoPassword']
posturl = config['posturl']
default_channel = config['default_channel']
slack_user = config['slack_user']
bring_interface_up = config.get('bring_interface_up', True)

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
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(event_host_ip, username=ciscoUsername, password=ciscoPassword)

    # Extract the config for the offending interface
    interface, intState = None, None
    event_message = os.environ.get('EVENT_MESSAGE', '')
    if event_message:
        logging.info(f"INFO: {Path(__file__).name} - Incoming event message: {event_message}")
        match = re.search(r"Interface (\S+), changed state to (\S+)", event_message)
        if match:
            interface, intState = match.groups()

    if not interface:
        logging.error(f"ERROR: {Path(__file__).name} - unable to obtain interface name from event message")
        exit(1)

    logging.info(f"Detected interface: {interface}, state: {intState}")

    # Try to get interface description
    output, error = execute_ssh_command(ssh, f"show running-config interface {interface} | include description")
    intDesc = output.replace("description ", "") if output else "Unable to retrieve interface description"
    logging.info(f"\n--CMD Output:\nshow running-config interface {interface} | include description:\n{intDesc}\n---\n")

    # Attempt to bring the interface up if it's down and the config allows it
    if intState == "down" and bring_interface_up:
        logging.info(f"Attempting to bring interface {interface} back up")
        commands = [
            "configure terminal",
            f"interface {interface}",
            "no shutdown",
            "exit",
            "exit"
        ]
        for cmd in commands:
            output, error = execute_ssh_command(ssh, cmd)
            if error:
                logging.error(f"Error executing command '{cmd}': {error}")
                break
        else:
            logging.info(f"Interface {interface} brought up successfully")

    # Post to Slack
    mnemonic = os.environ.get('EVENT_CISCO_MNEMONIC', '')
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    status_emoji = "🟢" if intState == "up" else "🔴"
    status_word = "RECOVERED" if intState == "up" else "DOWN"

    payload = {
        "username": slack_user,
        "icon_url": "http://www.logzilla.net/images/logo_orange_png_cropped_40x40.png",
        "blocks": [
            {
                "type": "header",
                "text": {
                    "type": "plain_text",
                    "text": f"{status_emoji} {status_word}: Interface {interface} on {event_host}"
                }
            },
            {
                "type": "section",
                "fields": [
                    {"type": "mrkdwn", "text": "*Device:*\n" + event_host},
                    {"type": "mrkdwn", "text": "*Interface:*\n" + interface},
                    {"type": "mrkdwn", "text": "*State:*\n" + intState},
                    {"type": "mrkdwn", "text": "*Description:*\n" + intDesc},
                    {"type": "mrkdwn", "text": "*Program:*\n" + os.environ.get('EVENT_PROGRAM', 'Unknown')},
                    {"type": "mrkdwn", "text": "*Severity:*\n" + os.environ.get('EVENT_SEVERITY', 'Unknown')},
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

    response = requests.post(posturl, json=payload, timeout=5, verify=False)

    if response.status_code != 200:
        logging.error(f"Request to Slack returned an error {response.status_code}, the response is:\n{response.text}")
    else:
        logging.info("Successfully posted to Slack")

    logging.debug(json.dumps(payload))

except paramiko.AuthenticationException:
    logging.error("Authentication failed. Please check your username and password.")
except paramiko.SSHException as ssh_ex:
    logging.error(f"Unable to establish SSH connection: {str(ssh_ex)}")
except Exception as e:
    logging.error(f"An unexpected error occurred: {str(e)}")
    logging.exception("Exception details:")
finally:
    if 'ssh' in locals():
        ssh.close()
