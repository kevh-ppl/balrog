# Balrog

### First, this is an attempt to learn about Linux/OSS programming

### Then, Balrog is a Linux daemon that detects, monitors, and gives the option to use a sandbox enviromentm for suspicious USB devices, preventing malware spread via removable drives. Designed as a learning project, but aiming for practical use

![Dev Build](https://github.com/kevh-ppl/balrog/actions/workflows/devs.yml/badge.svg?branch=devs)
![GitHub release (latest)](https://img.shields.io/github/v/release/kevh-ppl/balrog)
![GitHub repo size](https://img.shields.io/github/repo-size/kevh-ppl/balrog)
![License](https://img.shields.io/github/license/kevh-ppl/balrog)
![Platform](https://img.shields.io/badge/platform-linux-lightgrey)

![Little explanation on balrog shitty design](data/balrog.drawio.png)

## Debug

Must have installed dev version of libacl, glibc, libnotify and libudev.

```bash
git clone https://github.com/kevh-ppl/balrog.git
cd balrog
make debug
```

## Installation

You may want to use the prebuilt DEB package:

[Download Balrog DEB package](https://github.com/kevh-ppl/balrog/releases/download/0.1.0.v2025.12.12-9e374c2-alpha/balrog_0.1.0.v2025.12.12-9e374c2-alpha_amd64.deb)

## Roadmap

- [x] Basic daemon
- [x] USB detection via udev
- [x] User notifications via libnotify
- [x] Udev rules to disable automount
- [x] Sandboxing USB storage

Dios, ayúdame.
