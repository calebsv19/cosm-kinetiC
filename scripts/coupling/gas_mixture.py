"""Bounded ideal-mixture sensible thermodynamics, independently specified by NIST.

Reference evaluator only: not the native Euler EOS, chemical heat release,
transport, equilibrium chemistry, condensation or measured flame composition.
Dry air omits argon; wet methanol products specify complete stoichiometric
CH3OH + 1.5*(O2 + 3.76*N2) -> CO2 + 2*H2O + 5.64*N2.
No water-vapor extrapolation below its published 500 K fit.
"""
import math

R_UNIVERSAL = 8.31446261815324  # J/(mol K), SI molar gas constant
# (lower K, upper K, A..E); NIST Chemistry WebBook Shomate Cp in J/(mol K).
# Only the published bands needed for the bounded 298..1500 K gate are copied.
DATA = {
    'N2': (.0280134, [(100., 500., (28.98641, 1.853978, -9.647459, 16.63537, .000117)),
                      (500., 2000., (19.50583, 19.88705, -8.598535, 1.369784, .527601))]),
    'O2': (.0319988, [(100., 700., (31.32234, -20.23531, 57.86644, -36.50624, -.007374)),
                      (700., 2000., (30.03235, 8.772972, -3.988133, .788313, -.741599))]),
    'CO2': (.0440095, [(298., 1200., (24.99735, 55.18696, -33.69137, 7.948387, -.136638)),
                       (1200., 6000., (58.16639, 2.720074, -.492289, .038844, -6.447293))]),
    'H2O': (.0180153, [(500., 1700., (30.09200, 6.832514, 6.793435, -2.534480, .082139))]),
}
SOURCES = {name: 'https://webbook.nist.gov/cgi/cbook.cgi?ID=C'+cas+'&Mask=1&Type=JANAFG&Table=on'
           for name, cas in [('N2', '7727379'), ('O2', '7782447'), ('CO2', '124389'), ('H2O', '7732185')]}
DRY_AIR = {'N2': 3.76, 'O2': 1.}
METHANOL_PRODUCTS = {'N2': 5.64, 'CO2': 1., 'H2O': 2.}


def finite(value):
    if type(value) not in (int, float) or not math.isfinite(value):
        raise ValueError('finite physical number required')
    return value


def composition(moles):
    if type(moles) is not dict or not moles or set(moles)-set(DATA):
        raise ValueError('specified supported mole amounts required')
    if any(finite(v) <= 0 for v in moles.values()):
        raise ValueError('positive species mole amounts required')
    total = math.fsum(moles.values())
    mass = math.fsum(v*DATA[k][0] for k, v in moles.items())
    if not math.isfinite(total) or not math.isfinite(mass):
        raise ValueError('bounded mixture amounts required')
    return {k: v/total for k, v in moles.items()}, mass/total


def bounds(moles):
    composition(moles)
    return max(298., *(DATA[k][1][0][0] for k in moles)), 1500.


def band(name, temperature):
    if name not in DATA: raise ValueError('unsupported species')
    finite(temperature)
    for low, high, coefficients in DATA[name][1]:
        if low <= temperature <= high:
            return coefficients
    raise ValueError('temperature outside published species fit')


def cp_molar(name, temperature):
    a, b, c, d, e = band(name, temperature); t = temperature/1000
    return a+b*t+c*t*t+d*t*t*t+e/(t*t)


def integral(coefficients, temperature):
    a, b, c, d, e = coefficients; t = temperature/1000
    return 1000*(a*t+b*t*t/2+c*t*t*t/3+d*t**4/4-e/t)


def delta_h_molar(name, begin, end):
    band(name, begin); band(name, end)
    if end < begin: return -delta_h_molar(name, end, begin)
    terms = []
    for low, high, coefficients in DATA[name][1]:
        left, right = max(low, begin), min(high, end)
        if right > left: terms.append(integral(coefficients, right)-integral(coefficients, left))
    # Integrate piecewise Cp rather than adding the rounded fit's F-H jumps.
    return math.fsum(terms)


def properties(moles, temperature):
    fractions, mass = composition(moles); lo, hi = bounds(moles)
    if not lo <= finite(temperature) <= hi: raise ValueError('bounded mixture fit required')
    cp = math.fsum(v*cp_molar(k, temperature) for k, v in fractions.items())/mass
    gas_constant = R_UNIVERSAL/mass; cv = cp-gas_constant
    if cv <= 0: raise ValueError('positive mixture Cv required')
    return dict(cp_j_kg_k=cp, cv_j_kg_k=cv, gas_constant_j_kg_k=gas_constant,
                gamma=cp/cv, sound_speed_m_s=math.sqrt(cp/cv*gas_constant*temperature),
                mean_molar_mass_kg_mol=mass)


def delta_internal(moles, begin, end):
    fractions, mass = composition(moles)
    properties(moles, begin); properties(moles, end)
    return (math.fsum(v*delta_h_molar(k, begin, end) for k, v in fractions.items())
            -R_UNIVERSAL*(end-begin))/mass


def temperature_after_energy(moles, begin, energy_j_kg):
    properties(moles, begin); finite(energy_j_kg); low, high = bounds(moles)
    if not delta_internal(moles, begin, low) <= energy_j_kg <= delta_internal(moles, begin, high):
        raise ValueError('energy outside bounded caloric model')
    for _ in range(64):
        middle = (low+high)/2
        if delta_internal(moles, begin, middle) < energy_j_kg: low = middle
        else: high = middle
    return (low+high)/2
