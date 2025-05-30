#!/usr/bin/env bash

arr2js(){
  local arr=( "$@" );
  local len=${#arr[@]}
  if [[ ${len} -eq 0 ]]; then
    >&2 echo "Error: Length of input array needs to be at least 2.";
    return 1;
  fi
  if [[ $((len%2)) -eq 1 ]]; then
    >&2 echo "Error: Length of input array needs to be even (key/value pairs).";
    return 1;
  fi
  local data="";
  local foo=0;
  for i in "${arr[@]}"; do
    local char=","
    if [ $((++foo%2)) -eq 0 ]; then
      char=":";
    fi
    local first="${i:0:1}";  # read first charc
    local app="\"$i\""
    if [[ "$first" == "^" ]]; then
      app="${i:1}"  # remove first char
    fi
    data="$data$char$app";
  done
  data="${data:1}";  # remove first char
  echo "{$data}";    # add braces around the string
}


#### now use it like so:
# arr2js a 3 c true
#  {"a":"3","c":"true"}
# also works with numbers and booleans
# arr2js a ^3 c ^true
#  {"a":3,"c":true}
