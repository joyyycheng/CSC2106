# Assisted Home Automation for the Elderly

## Introduction
This project aims to develop an assisted home automation system specifically designed for the elderly. The system will provide various features to enhance the comfort, safety, and convenience of elderly individuals living independently at home.

## Features
Notifications Alert based on the sensors
Able to integrate multiple sensors for the system(Presence, Heart Rate, Temperature)

## Installation
There are a few devices to be set up, please navigate to the respective header for your device

### Raspberry Pi
This Raspberry Pi will be set up with Home Assistant & Docker (Full guide on [this website](https://www.tim-kleyersburg.de/articles/home-assistant-with-docker-2023/)).

1. Install Rasberry Pi OS with Raspberry Pi Imager
2. Install Docker
3. Run Home assistant as Docker container
4. Install mosquitto broker as a docker container [website](https://www.homeautomationguy.io/blog/docker-tips/configuring-the-mosquitto-mqtt-docker-container-for-use-with-home-assistant)
5. Setup the MQTT via the integration tab 
6. SSH the `configuration.yaml` and `automation.yaml` file for the setup.


## Usage


## Contributing


