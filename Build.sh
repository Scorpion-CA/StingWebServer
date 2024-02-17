#!/bin/bash

# Builds the server
g++ -o Output/Output.out Main.cpp HTTP.cpp Connections.cpp -pthread
