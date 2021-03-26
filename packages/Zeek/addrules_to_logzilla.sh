#!/bin/bash

for r in rules.d/*.yaml; do logzilla rules add $r -f -R; done
logzilla rules add 401-zeek-portmap-dst.yaml
logzilla rules add 401-zeek-portmap-src.yaml
logzilla rules reload

