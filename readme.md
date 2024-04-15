# Interactive SSH client with in-device WiFi and SSH connection setup

## Overview
Stole and modified a SSH client for the M5Cardputer so you can connect to WiFi and different SSH hosts dinamically from the cardputer itself.

Original project from https://github.com/fernandofatech/M5Cardputer-SSHClient

![IMG_3531](IMG_20240330_015103.jpg)

## How to use
A: Download the .ino file and flash it with your favorite tool

B: Use the M5Launcher and download the .bin file and put it in the MicroSD card

Every time it boots it will ask for SSID and its password, and if the connection is successful it will then ask if you want to use WireGuard VPN (WIP) and finally for the  SSH host, username and password.

Put the Wireguard .conf file on the root of the sdcard and name it "wg.conf"

## WIP features
WireGuard VPN support

The code implementation is mostly done, but i'm having issues making the WireGuard-ESP32 library work, it keeps looping on attempting the handshake. Trying the official demo for the library I observe the same behaviour.

I will investigate this once I have more time.

## FYI
I just get tangled on those silly projects from time to time and i drop them immediately. Dont expect any future support.
