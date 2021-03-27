#!/bin/bash

for r in rules.d/*.yaml; do logzilla rules add $r -f -R; done
for r in static/*.yaml; do logzilla rules add $r -f -R; done
logzilla rules reload

