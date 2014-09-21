# Introduction

A quick I/Q spectrum server for HackRF. This will allow you to control your HackRF and stream data from it over the network using TCP.

hackrf_tcp was coded in a rush just as a PoC and uploaded to github for the record. It will be abandoned in favour of osmorsdr_tcp which will work with any SDR supported by gr-osmosdr.

Right now the only way to connect/use hackrf_tcp is through a custom source block for gr-osmosdr which can be found at https://github.com/jpenalbae/gr-osmosdr_hackrftcp


# Known Issues

  * Tested only with Linux and OSX
  * Needs manual restart each time a client disconnects

# Dependencies

  * libhackrf https://github.com/mossmann/hackrf

# Build

    $ make
    
# Usage

    $ ./hackrf_tcp -h
    hackrf_tcp, an I/Q spectrum server for HackRF
    
    Usage:  [-a listen address]
            [-p listen port (default: 1234)]
            [-f frequency to tune to [Hz]]
            [-s samplerate in Hz (default: 8000000 Hz)]
