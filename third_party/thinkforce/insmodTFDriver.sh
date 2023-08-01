#!/bin/bash

base_dir=$(
  cd "$(dirname "$0")" || exit
  pwd
)

cd "$base_dir" || exit

operator=${1:-"insmod"}
ip=${2:-"none"}

if [[ ${operator} == "insmod" ]]; then
  lsmod | grep tfacc2 >/dev/null
  if [[ $? -eq 0 ]]; then
    echo "tfacc2 has already insmod."
  else
    is_insmod_driver=$(modprobe tfacc2 >/dev/null 2>&1)
    if [[ $? != 0 ]]; then
      cd ./driver/tfacc2 || exit
      if [[ ${ip} == "none" ]]; then
        ./build_driver.sh
        modprobe tfacc2
        echo "tfacc2 insmod success."
      else
        ./build_driver.sh "${ip}"
        modprobe tfacc2
        echo "tfacc2 insmod success."
      fi
    else
      echo "tfacc2 insmod success."
    fi
  fi
elif [[ ${operator} == "rm" ]]; then
  lsmod | grep tfacc2 >/dev/null
  if [[ $? -ne 0 ]]; then
    echo "tfacc2 has not insmod."
    exit 0
  fi
  modprobe -r tfacc2
  echo 'tfacc2 remove success.'
elif [[ ${operator} == "delete" ]]; then
  find /lib/modules/ -name "tfacc2.ko" -type f -delete
  echo 'tfacc2 delete success.'
else
  echo 'Nothing to happen.'
fi
