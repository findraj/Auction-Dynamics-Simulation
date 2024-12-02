#!/bin/bash

# Run model 100 times
for i in {1..100}
do
    ./model -i 100 -b 70 -d 60
done