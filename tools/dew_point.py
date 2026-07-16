#!/usr/bin/env python3
import math, argparse
p=argparse.ArgumentParser(); p.add_argument("temperature",type=float); p.add_argument("humidity",type=float); a=p.parse_args()
g=math.log(max(1,min(100,a.humidity))/100)+17.62*a.temperature/(243.12+a.temperature)
print(round(243.12*g/(17.62-g),3))
