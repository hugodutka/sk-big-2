#!/bin/bash
rm -rf ./zadanie2
mkdir -p ./zadanie2/proxy
mkdir -p ./zadanie2/client
cp proxy/*.cc proxy/*.hh ./zadanie2/proxy
cp client/*.cc client/*.hh ./zadanie2/client
cp makefile ./zadanie2
tar czvf zadanie2.tgz ./zadanie2
rm -rf ./zadanie2
