projectM Qt-based Standalone Application
========================================

This repository contains the sources of the Qt-based standalone application. This application can be used to run
projectM on your local system's audio devices. It uses Qt for the graphical frontend, providing a few more features than
the SDL-based application:

- Preset playlist editor with XML-based load and save functionality
- Changing and remembering preset ratings per playlist
- A graphical dialog to change the projectM settings

Currently, the application runs on Linux-based desktops using PulseAudio, PipeWire, or JACK as audio backends.

**PipeWire Support**: The new PipeWire backend provides native integration with modern Linux audio systems. PipeWire is the recommended audio backend for most users running recent Linux distributions.