"""Independent continuous finite-volume scalar and gas/object ODE references.

No native solver, pair-transfer algorithm, data normalization or fitting.
"""
import math

def scalar_cell_means(side,time,velocity,alpha):
    if side<4 or time<0 or alpha<0:raise ValueError('reference domain')
    wave=2*math.pi;h=1/side
    amplitude=.25*math.sin(wave*h/2)/(wave*h/2)*math.exp(-alpha*wave*wave*time)
    return [1+amplitude*math.cos(wave*((i+.5)*h-.25-velocity*time)) for i in range(side)]

def reservoir(time,gas_initial,object_initial,gas_capacity,object_capacity,conductance):
    if time<0 or min(gas_capacity,object_capacity)<=0 or conductance<0:raise ValueError('reference capacities/time')
    equilibrium=(gas_capacity*gas_initial+object_capacity*object_initial)/(gas_capacity+object_capacity)
    difference=(gas_initial-object_initial)*math.exp(-conductance*(1/gas_capacity+1/object_capacity)*time)
    return (equilibrium+object_capacity/(gas_capacity+object_capacity)*difference,
            equilibrium-gas_capacity/(gas_capacity+object_capacity)*difference)
