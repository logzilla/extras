#!/bin/bash
fio --output=fio-4k-$(hostname)-$(date +%s).txt 4ktest.fio
