import os
import requests
import time

if __name__ == '__main__':
    # pixel4_ip = '10.11.253.47'
    ip = '10.11.252.99' # oneplus
    # mi9_ip = "10.11.230.38"
    # sampler = 'thermal_battery,bms,pm8150_tz,aoss0-usr,cpuss-0-usr,cpuss-1-usr,gpuss-0-usr,gpuss-1-usr,power_temp,power_capacity,power_uevent'
    ts = int(time.time())    
    # sampler = 'thermal_battery,thermal_pm8150_tz,thermal_aoss0-usr,thermal_gpuss-0-usr,thermal_gpuss-1-usr,power_uevent,kgsl_gpubusy,kgsl_gpu_busy_percentage,kgsl_clock_mhz'
    while True:
        sampler = 'kgsl_gpu_clock_stats'
        data = requests.get(f"http://{ip}:8080/kgsl/" + sampler).content.decode('utf-8')
        print(data)
        time.sleep(0.2)