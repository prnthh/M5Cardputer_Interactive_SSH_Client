# Interactive SSH client with in-device WiFi and SSH connection setup

## Overview
Forked a SSH client for the M5Cardputer so you can connect to WiFi and different SSH hosts dinamically from the cardputer itself with support for saving and loading the WiFi and SSH credentiasl from a file.

Original project from https://github.com/fernandofatech/M5Cardputer-SSHClient

![IMG_3531](IMG_20240330_015103.jpg)

## Prerequisites

Before using this application, make sure you have the following:

- An M5Cardputer.
- A microSD card formatted FAT32.
- WiFi credentials (SSID and password) for the network you want to connect to.
- SSH server credentials (hostname, username, and password) for the remote server you want to access.
- (WIP) WireGuard configuration file (`wg.conf`) if you plan to use the VPN functionality. (WIP)

## Usage

2. The application will prompt you to choose whether to use saved WiFi and SSH credentials or input them manually.
3. If you choose to use saved credentials, the application will attempt to load them from the `/sshclient/session.wifi` and `/sshclient/session.ssh` files on the microSD card. !!CURRENTLY THE CREDENTIALS ARE SAVED IN PLAIN TEXT!!
4. If the credential files are not found or if you choose to input manually, you will be prompted to enter the WiFi SSID, password, SSH hostname, username, and password.
5. After entering the credentials, you will be asked if you want to save them to the respective files on the microSD card.
6. The application will attempt to connect to the WiFi network and the remote SSH server.
7. If you have a WireGuard configuration file (`/sshclient/wg.conf`) on the microSD card, you will be prompted to use the WireGuard VPN. Select "Y" to enable the VPN or "N" to continue without it.
8. Once connected, you can interact with the remote SSH server using the M5Cardputer's keyboard and display.

## Limitations

- This application does not support file transfers or advanced SSH features beyond interactive terminal sessions.
- The WireGuard VPN implementation is WIP. The code implementation is mostly done, but i'm having issues making the WireGuard-ESP32 library work, it keeps looping on attempting the handshake. Trying the official demo for the library I observe the same behaviour.

## FYI

I just get tangled on those silly projects from time to time and i drop them immediately. Dont expect any future support.
