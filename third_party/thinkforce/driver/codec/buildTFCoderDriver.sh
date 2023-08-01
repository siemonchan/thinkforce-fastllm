#!/bin/bash

base_dir=$(
  cd "$(dirname "$0")" || exit
  pwd
)

cd "$base_dir" || exit

operator=${1:-"build"}
ip=${2:-"none"}
is_pure_system=$(ls /banner >/dev/null 2>&1)

if [[ $? != 0 ]]; then
  type=$(ls /sys/bus | grep acpi)
  cpuCount=$(nproc)
  driverCount=3

  if ((0 < $((cpuCount)))) && (($((cpuCount)) <= 40)); then
    driverCount=3
  elif ((40 < $((cpuCount)))) && (($((cpuCount)) <= 80)); then
    driverCount=6
  else
    driverCount=12
  fi

  echo 'cpuCount:'"${cpuCount}"_"${driverCount}"

  TFCodecDriverList=$(ls -l | grep ^d | awk -F ' ' '{print $9}')
  if [[ ${operator} == "build" ]]; then
    cp -v ../../tfdec/firmware/* /lib/firmware/
    for i in ${TFCodecDriverList}; do
      if (($((driverCount)) >= 0)); then
        cd ./"${i}" || exit

        if [[ ${ip} == "none" ]]; then
          output_path=result
        else
          output_path=result/"${ip}"
        fi

        rm -rf "${output_path}"
        mkdir -p "${output_path}"
        cp -r $(ls | grep -v result | xargs) "${output_path}"
        cd - || exit

        cd "${i}"/"${output_path}" || exit

        export LINUX_VERSION=$(cat /etc/issue | awk -F ' ' '{print $1}' | awk 'NR==1')
        make

        mod_index=$(echo "${i}" | awk -F '-' '{print $2}')
        if [[ ${mod_index} == "e200" ]]; then
          insmod al_module.ko
          echo "TFcodec al_module insmod success."
        else
          insmod mve_driver"${mod_index}".ko
          echo "TFcodec mve_driver""${mod_index}"" insmod success."
        fi
        cd - || exit
        ((driverCount = driverCount - 1))
      fi
    done
  elif [[ ${operator} == "insmod" ]]; then
    for i in ${TFCodecDriverList}; do

      if (($((driverCount)) >= 0)); then
        if [[ ${ip} == "none" ]]; then
          output_path=result
        else
          output_path=result/"${ip}"
        fi

        cd ./"${i}"/"${output_path}" || exit
        mod_index=$(echo "${i}" | awk -F '-' '{print $2}')
        if [[ ${mod_index} == "e200" ]]; then
          insmod al_module.ko
          echo "TFcodec al_module insmod success."
        else
          insmod mve_driver"${mod_index}".ko
          echo "TFcodec mve_driver""${mod_index}"" insmod success."
        fi
        cd - || exit
        ((driverCount = driverCount - 1))
      fi

    done
  elif [[ ${operator} == "clean" ]]; then
    for i in ${TFCodecDriverList}; do

      if (($((driverCount)) >= 0)); then
        if [[ ${ip} == "none" ]]; then
          output_path=result
        else
          output_path=result/"${ip}"
        fi

        cd ./"${i}"/"${output_path}" || exit
        make clean
        cd - || exit
        ((driverCount = driverCount - 1))
      fi

    done

  elif [[ ${operator} == "rm" ]]; then
    for i in ${TFCodecDriverList}; do
      if (($((driverCount)) >= 0)); then
        if [[ ${ip} == "none" ]]; then
          output_path=result
        else
          output_path=result/"${ip}"
        fi

        cd ./"${i}"/"${output_path}" || exit
        mod_index=$(echo "${i}" | awk -F '-' '{print $2}')
        if [[ ${mod_index} == "e200" ]]; then
          rmmod al_module.ko
          if [[ "$?" == 0 ]]; then
            echo 'remove al_module.ko success.'
          fi
        else
          rmmod mve_driver"${mod_index}".ko
          if [[ "$?" == 0 ]]; then
            echo 'remove 'mve_driver"${mod_index}".ko' success.'
          fi
        fi
        cd - || exit
        ((driverCount = driverCount - 1))
      fi
    done
  fi
elif [[ ${operator} == "update" ]]; then
  cp -v ../../tfdec/firmware/* /lib/firmware/
  echo 'Your tfdec firmware is updated.'
else
  echo 'Your NPU40T is not need custom TFCodec drivers.'
fi
