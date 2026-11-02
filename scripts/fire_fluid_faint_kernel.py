"""Positive half-line Robin arrival kernel and conservative solid-feedback bound.

The x mean removes prescribed shear exactly. The source has width fraction 1/8.
For dT/dz=kappa*T at z=0, K(0,z,t) = exp(-a*a) *
 [1/sqrt(pi*alpha*t) - kappa*erfcx(a+kappa*sqrt(alpha*t))].
This is the boundary Green function, integrated over the original source and
its finite heating duration. Actual net heat is bounded by cold-wall heat and
the maximal re-emission allowed by the solid's 50 J/K surface capacity.
"""
import math
import numpy as np

ALPHA = .026 / 1206
H = 1 / (.1 + .005 / (2 * .16))
AREA = .0625


def erfcx(x):
    """Scaled complementary error function for positive arguments only."""
    def scalar(v):
        if v < 20:
            return math.exp(v*v) * math.erfc(v)
        term = total = 1.
        for j in range(1, 12):
            term *= -(2*j-1) / (2*v*v)
            total += term
        return total / (math.sqrt(math.pi) * v)
    return np.vectorize(scalar, otypes=[float])(x)


def arrival(t, points=128):
    if t == 0:
        return dict(cold_q_j=0., feedback_bound_j=0., top_image_bound_j=0.,
                    lower_q_j=0., upper_q_j=0.)
    x, w = np.polynomial.legendre.leggauss(points)
    age = t * (x + 1) / 2
    aw = t * w / 2
    z = 1 / 64 + (x + 1) / 128
    zw = w / 128
    root = np.sqrt(ALPHA * age[:, None])
    a = z[None, :] / (2 * root)
    kappa = H / .026
    kernel = np.exp(-a*a) * (1 / (np.sqrt(np.pi) * root)
                             - kappa * erfcx(a + kappa * root))
    assert np.isfinite(kernel).all() and (kernel >= 0).all()
    normal = kernel @ zw
    duration = t - age
    cold = H * AREA / 8 * np.dot(aw * np.minimum(.4, duration), normal)
    integral_duration = np.where(duration <= .4, duration**2 / 2,
                                 .4 * duration - .08)
    feedback = H * AREA / 50 * H * AREA / 8 * np.dot(
        aw * integral_duration, normal)
    # An upper estimate from fully reflecting images bounds top-wall influence.
    images = np.zeros_like(normal)
    for p in range(1, 5):
        for sign in [-1, 1]:
            distance = 2 * p * .0625 + sign * z[None, :]
            images += (np.exp(-(distance / (2 * root))**2)
                       / (np.sqrt(np.pi) * root)) @ zw
    top = H * AREA / 8 * np.dot(aw * np.minimum(.4, duration), images)
    return dict(cold_q_j=float(cold), feedback_bound_j=float(feedback),
                top_image_bound_j=float(top), lower_q_j=float(cold - feedback),
                upper_q_j=float(cold + top))


def free_shear_tail(t, points=48):
    """Positive infinite-domain check; finite-wall qualification is separate.

    For linear shear a*z, conditional X|Z has mean xs+a*t*(zs+z)/2
    and variance 2*alpha*t+alpha*a*a*t**3/6. Integrate the original source
    and target without subtracting Fourier modes. This is NOT a wall oracle.
    """
    if t == 0:
        return 0.
    x, w = np.polynomial.legendre.leggauss(points)
    zs = 1 / 64 + (x + 1) / 128
    zw = w / 128
    alo = max(0, t - .4)
    ages = alo + (t - alo) * (x + 1) / 2
    aw = (t - alo) * w / 2
    result = 0.
    for age, weight in zip(ages, aw):
        normal = np.exp(-(zs[:, None] - zs[None, :])**2 / (4*ALPHA*age))
        normal /= np.sqrt(4 * np.pi * ALPHA * age)
        shift = .8 * age * (zs[:, None] + zs[None, :]) / 2
        width = np.sqrt(4*ALPHA*age + ALPHA*.8**2*age**3/3)
        def primitive(distance):
            v = distance / width
            return width/(2*np.sqrt(np.pi)) * np.exp(-v*v) * (
                1 - np.sqrt(np.pi)*v*erfcx(v))
        rectangle = (primitive(3/32 - 1/16 - shift)
                     - primitive(1/8 - 1/16 - shift)
                     - primitive(3/32 - 1/32 - shift)
                     + primitive(1/8 - 1/32 - shift))
        assert (rectangle >= 0).all()
        result += weight * np.sum(normal * rectangle
                                   * zw[:, None] * zw[None, :]) / ((1/32)*(1/64))
    return float(result)
